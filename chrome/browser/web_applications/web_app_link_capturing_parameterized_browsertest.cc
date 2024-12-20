// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>
#include <string>
#include <string_view>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/current_thread.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_restrictions.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_permission_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/web_apps/web_app_link_capturing_test_utils.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/mojom/manifest/manifest_launch_handler.mojom-shared.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace web_app {

namespace {

constexpr char kStartPageScopeA[] =
    "/banners/link_capturing/scope_a/start.html";
constexpr char kDestinationPageScopeA[] =
    "/banners/link_capturing/scope_a/destination.html";
constexpr char kDestinationPageScopeB[] =
    "/banners/link_capturing/scope_b/destination.html";
constexpr char kDestinationPageScopeX[] =
    "/banners/link_capturing/scope_x/destination.html";
constexpr char kLinkCaptureTestInputPathPrefix[] = "chrome/test/data/web_apps/";

constexpr char kValueScopeA2A[] = "A_TO_A";
constexpr char kValueScopeA2B[] = "A_TO_B";
constexpr char kValueScopeA2X[] = "A_TO_X";
constexpr char kValueLink[] = "LINK";
constexpr char kValueButton[] = "BTN";
constexpr char kValueFormButton[] = "FORM_BTN";
constexpr char kValueServiceWorkerButton[] = "BTN_SW";
constexpr char kValueOpener[] = "OPENER";
constexpr char kValueNoOpener[] = "NO_OPENER";
constexpr char kValueTargetSelf[] = "SELF";
constexpr char kValueTargetFrame[] = "FRAME";
constexpr char kValueTargetBlank[] = "BLANK";
constexpr char kValueTargetNoFrame[] = "NO_FRAME";

// Whether Link capturing is turned on:
enum class LinkCapturing {
  kEnabled,
  kDisabled,
  kEnabledViaClientMode,
};

constexpr std::string_view ToParamString(LinkCapturing capturing) {
  switch (capturing) {
    case LinkCapturing::kEnabled:
      return "CaptureOn";
    case LinkCapturing::kDisabled:
      return "CaptureOff";
    case LinkCapturing::kEnabledViaClientMode:
      return "CaptureForNonAuto";
  }
}

// The user display mode configuration for the apps.
enum class AppUserDisplayMode {
  // Both apps are UserDisplayMode::kBrowser.
  kBothBrowser,
  // Both apps are UserDisplayMode::kStandalone.
  kBothStandalone,
  // App A is UserDisplayMode::kStandalone, and App B is
  // UserDisplayMode::kBrowser.
  kAppAStandaloneAppBBrowser,
  kMaxValue = kAppAStandaloneAppBBrowser,
};

constexpr std::string_view ToParamString(AppUserDisplayMode mode) {
  switch (mode) {
    case AppUserDisplayMode::kBothBrowser:
      return "BothBrowser";
    case AppUserDisplayMode::kBothStandalone:
      return "BothStandalone";
    case AppUserDisplayMode::kAppAStandaloneAppBBrowser:
      return "AppAStandaloneAppBBrowser";
  }
}

// The starting point for the test:
enum class StartingPoint {
  kAppWindow,
  kTab,
};

constexpr std::string_view ToParamString(StartingPoint start) {
  switch (start) {
    case StartingPoint::kAppWindow:
      return "AppWnd";
    case StartingPoint::kTab:
      return "Tab";
  }
}

// Destinations:
// ScopeA2A: Navigation to an installed app, within same scope.
// ScopeA2B: Navigation to an installed app, but different scope.
// ScopeA2X: Navigation to non-installed app (different scope).
enum class Destination {
  kScopeA2A,
  kScopeA2B,
  kScopeA2X,
};

constexpr std::string ToIdString(Destination scope) {
  switch (scope) {
    case Destination::kScopeA2A:
      return kValueScopeA2A;
    case Destination::kScopeA2B:
      return kValueScopeA2B;
    case Destination::kScopeA2X:
      return kValueScopeA2X;
  }
}

constexpr std::string_view ToParamString(Destination scope) {
  switch (scope) {
    case Destination::kScopeA2A:
      return "ScopeA2A";
    case Destination::kScopeA2B:
      return "ScopeA2B";
    case Destination::kScopeA2X:
      return "ScopeA2X";
  }
}

enum class RedirectType {
  kNone,
  kServerSideViaA,
  kServerSideViaB,
  kServerSideViaX,
};

std::string ToIdString(RedirectType redirect, Destination final_destination) {
  switch (redirect) {
    case RedirectType::kNone:
      return ToIdString(final_destination);
    case RedirectType::kServerSideViaA:
      return kValueScopeA2A;
    case RedirectType::kServerSideViaB:
      return kValueScopeA2B;
    case RedirectType::kServerSideViaX:
      return kValueScopeA2X;
  }
}

constexpr std::string_view ToParamString(RedirectType redirect) {
  switch (redirect) {
    case RedirectType::kNone:
      return "Direct";
    case RedirectType::kServerSideViaA:
      return "ServerSideViaA";
    case RedirectType::kServerSideViaB:
      return "ServerSideViaB";
    case RedirectType::kServerSideViaX:
      return "ServerSideViaX";
  }
}

// The element to use for navigation:
enum class NavigationElement {
  kElementLink,
  kElementButton,
  kElementFormPost,
  kElementServiceWorkerButton,
  kElementIntentPicker,
};

std::string ToIdString(NavigationElement element) {
  switch (element) {
    case NavigationElement::kElementLink:
      return kValueLink;
    case NavigationElement::kElementButton:
      return kValueButton;
    case NavigationElement::kElementFormPost:
      return kValueFormButton;
    case NavigationElement::kElementServiceWorkerButton:
      return kValueServiceWorkerButton;
    case NavigationElement::kElementIntentPicker:
      // The IntentPicker is within the Chrome UI, not the web page. Therefore,
      // this should not be used to construct an ID to click on within the page.
      NOTREACHED();
  }
}

constexpr std::string_view ToParamString(NavigationElement element) {
  switch (element) {
    case NavigationElement::kElementLink:
      return "ViaLink";
    case NavigationElement::kElementButton:
      return "ViaButton";
    case NavigationElement::kElementServiceWorkerButton:
      return "ViaServiceWorkerButton";
    case NavigationElement::kElementIntentPicker:
      return "ViaIntentPicker";
    case NavigationElement::kElementFormPost:
      return "ViaFormPost";
  }
}

constexpr std::string_view ToParamString(test::ClickMethod click) {
  switch (click) {
    case test::ClickMethod::kLeftClick:
      return "LeftClick";
    case test::ClickMethod::kMiddleClick:
      return "MiddleClick";
    case test::ClickMethod::kShiftClick:
      return "ShiftClick";
    case test::ClickMethod::kRightClickLaunchApp:
      return "RightClick";
  }
}

// Whether to supply an Opener/NoOpener:
enum class OpenerMode {
  kOpener,
  kNoOpener,
};

constexpr std::string_view ToIdString(OpenerMode opener) {
  switch (opener) {
    case OpenerMode::kOpener:
      return kValueOpener;
    case OpenerMode::kNoOpener:
      return kValueNoOpener;
  }
}

// ClientMode combinations for apps A and B used in the test suite. For enum
// values that match the default launch handling client modes, both apps get the
// same client mode.
enum class ClientModeCombination {
  kAuto,
  kBothNavigateNew,
  kBothNavigateExisting,
  kBothFocusExisting,
  kAppANavigateExistingAppBFocusExisting,
};

std::string ToParamString(ClientModeCombination client_mode_combo) {
  switch (client_mode_combo) {
    case ClientModeCombination::kAuto:
      return "";
    case ClientModeCombination::kBothFocusExisting:
      return "FocusExisting";
    case ClientModeCombination::kBothNavigateNew:
      return "NavigateNew";
    case ClientModeCombination::kBothNavigateExisting:
      return "NavigateExisting";
    case ClientModeCombination::kAppANavigateExistingAppBFocusExisting:
      return "AppANavigateExistingAppBFocusExisting";
  }
}

constexpr std::string_view ToParamString(OpenerMode opener) {
  switch (opener) {
    case OpenerMode::kOpener:
      return "WithOpener";
    case OpenerMode::kNoOpener:
      return "WithoutOpener";
  }
}

template <typename ParamType, class Char, class Traits>
std::basic_ostream<Char, Traits>& operator<<(
    std::basic_ostream<Char, Traits>& os,
    ParamType param) {
  return os << ToParamString(param);
}

// The target to supply for the navigation:
enum class NavigationTarget {
  // The target to supply for the navigation:
  kSelf,
  kFrame,
  kBlank,
  kNoFrame,
};

constexpr std::string_view ToIdString(NavigationTarget target) {
  switch (target) {
    case NavigationTarget::kSelf:
      return kValueTargetSelf;
    case NavigationTarget::kFrame:
      return kValueTargetFrame;
    case NavigationTarget::kBlank:
      return kValueTargetBlank;
    case NavigationTarget::kNoFrame:
      return kValueTargetNoFrame;
  }
}

constexpr std::string_view ToParamString(NavigationTarget target) {
  switch (target) {
    case NavigationTarget::kSelf:
      return "TargetSelf";
    case NavigationTarget::kFrame:
      return "TargetFrame";
    case NavigationTarget::kBlank:
      return "TargetBlank";
    case NavigationTarget::kNoFrame:
      return "TargetNoFrame";
  }
}

// Use a std::tuple for the overall test configuration so testing::Combine can
// be used to construct the values.
//
// Since this configuration is rather long (and can make the test expectations
// less readable), this tuple is split into the ExpectationsFileConfig
// and ShortenedTestConfig tuples. The former is used to split & add a
// suffix to test expectation file names, and the latter are the 'remaining'
// configuration items that can constitute a more 'shortened' name (as they
// don't need to include the file configuration params).
//
// Note: Adding a value here needs to be accompanied with adding that value to
// either ExpectationsFileConfig or ShortenedTestConfig.
using LinkCaptureTestParam = std::tuple<ClientModeCombination,
                                        AppUserDisplayMode,
                                        LinkCapturing,
                                        StartingPoint,
                                        Destination,
                                        RedirectType,
                                        NavigationElement,
                                        test::ClickMethod,
                                        OpenerMode,
                                        NavigationTarget>;

// Test files are split by these configurations, to improve readability. When
// updating this config, the following methods need to be updated:
// - `GetExpectationsFileSuffix`
// - `RemoveExpectationsFileConfigFromTestName`
// - `GetExpectationsFileConfigFromTestConfig`
// - Every "Cleanup" test needs to be updated to use all new possible
//   combination of values.
// Each test fixture is currently split with:
// - The 'app user display mode' configuration, which controls what the user's
//   desired display mode is for each app.
// - The 'link capturing' user setting being on or off. This appends
//   "_capture_on" or "_capture_off" to the test fixture's test expectation file
//   base name.
using ExpectationsFileConfig = std::tuple<AppUserDisplayMode, LinkCapturing>;

// This is the 'rest' of the LinkCaptureTestParam configuration after the file
// configuration is removed.
using ShortenedTestConfig = std::tuple<ClientModeCombination,
                                       StartingPoint,
                                       Destination,
                                       RedirectType,
                                       NavigationElement,
                                       test::ClickMethod,
                                       OpenerMode,
                                       NavigationTarget>;

ExpectationsFileConfig GetExpectationsFileConfigFromTestConfig(
    const LinkCaptureTestParam& test_config) {
  return {std::get<AppUserDisplayMode>(test_config),
          std::get<LinkCapturing>(test_config)};
}

ShortenedTestConfig GetShortenedConfigFromTestConfig(
    const LinkCaptureTestParam& test_config) {
  return {
      std::get<ClientModeCombination>(test_config),
      std::get<StartingPoint>(test_config),
      std::get<Destination>(test_config),
      std::get<RedirectType>(test_config),
      std::get<NavigationElement>(test_config),
      std::get<test::ClickMethod>(test_config),
      std::get<OpenerMode>(test_config),
      std::get<NavigationTarget>(test_config),
  };
}

template <typename Tuple>
std::string TupleToParamString(const Tuple& params) {
  // Concatenates the result of calling `ToParamString()` on each member of the
  // tuple with '_' in between fields.
  std::string name = std::apply(
      [](auto&... p) { return base::JoinString({ToParamString(p)...}, "_"); },
      params);
  base::TrimString(name, "_", &name);
  return name;
}

template <typename TupleItemType, typename Tuple>
std::string TupleItemToParamString(const Tuple& tuple) {
  return std::string(ToParamString(std::get<TupleItemType>(tuple)));
}

// Returns the suffix to be appended to the base file name given the
// `file_config`. This must be unique for each possible value of the
// `ExpectationsFileConfig` type.
std::string GetExpectationsFileSuffix(
    const ExpectationsFileConfig& file_config) {
  return "_" + TupleToParamString(file_config);
}

// Returns whether the `file_config` configuration will contain the given
// `full_test_params` from the gtest name.
bool DoesTestMatchFileConfig(std::string_view full_test_params,
                             const ExpectationsFileConfig& file_config) {
  std::string link_capturing_name =
      TupleItemToParamString<LinkCapturing>(file_config);
  std::string display_mode_name =
      TupleItemToParamString<AppUserDisplayMode>(file_config);
  return base::Contains(full_test_params, link_capturing_name) ||
         base::Contains(full_test_params, display_mode_name);
}

// Removes all of the parameters from the `full_test_params` that are handled by
// the `file_config`. This is equivalent to
// TupleToParamString(GetShortenedConfigFromTestConfig(GetParams())), but when
// analyzing the testing::TestInfo object we only have access to the string
// version of the params, so this method is needed.
std::string RemoveExpectationsFileConfigFromFullTestParams(
    std::string_view full_test_params,
    const ExpectationsFileConfig& file_config) {
  std::string link_capturing_name =
      TupleItemToParamString<LinkCapturing>(file_config);
  std::string display_mode_name =
      TupleItemToParamString<AppUserDisplayMode>(file_config);
  std::string output(full_test_params);
  base::ReplaceSubstringsAfterOffset(&output, 0, link_capturing_name + "_", "");
  base::ReplaceSubstringsAfterOffset(&output, 0, display_mode_name + "_", "");
  return output;
}

std::string BrowserTypeToString(Browser::Type type) {
  switch (type) {
    case Browser::Type::TYPE_NORMAL:
      return "TYPE_NORMAL";
    case Browser::Type::TYPE_POPUP:
      return "TYPE_POPUP";
    case Browser::Type::TYPE_APP:
      return "TYPE_APP";
    case Browser::Type::TYPE_DEVTOOLS:
      return "TYPE_DEVTOOLS";
    case Browser::Type::TYPE_APP_POPUP:
      return "TYPE_APP_POPUP";
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case Browser::Type::TYPE_CUSTOM_TAB:
      return "TYPE_CUSTOM_TAB";
#endif
    case Browser::Type::TYPE_PICTURE_IN_PICTURE:
      return "TYPE_PICTURE_IN_PICTURE";
  }
  NOTREACHED() << "Unknown browser type: " + base::NumberToString(type);
}

bool IsNewTabOrAboutBlankUrl(const Browser* browser, const GURL& url) {
  return url == GURL("about:blank") || url == GURL("chrome://newtab") ||
         url == GURL("chrome://new-tab-page") || url == browser->GetNewTabURL();
}

// Serializes the state of a RenderFrameHost relevant for this test into a
// dictionary that can be stored as JSON. This includes the frame name and
// current URL.
// TODO(crbug.com/359418631): Add opener information to frames if possible.
base::Value::Dict RenderFrameHostToJson(content::RenderFrameHost& rfh) {
  base::Value::Dict dict;
  if (!rfh.GetFrameName().empty()) {
    dict.Set("frame_name", rfh.GetFrameName());
  }
  dict.Set("current_url", rfh.GetLastCommittedURL().PathForRequest());
  return dict;
}

// Serializes the state of a WebContents, including the state of all its iframes
// as well as navigation history for the tab.
base::Value::Dict WebContentsToJson(const Browser& browser,
                                    content::WebContents& web_contents) {
  base::Value::Dict dict =
      RenderFrameHostToJson(*web_contents.GetPrimaryMainFrame());
  if (web_contents.HasOpener()) {
    dict.Set("has_opener", true);
  }

  GURL last_committed_url =
      web_contents.GetPrimaryMainFrame()->GetLastCommittedURL();

  // The new tab page has inconsistent frames, so skip frame analysis there.
  if (!IsNewTabOrAboutBlankUrl(&browser, last_committed_url)) {
    base::Value::List frames;
    web_contents.GetPrimaryMainFrame()->ForEachRenderFrameHost(
        [&](content::RenderFrameHost* frame) {
          if (frame->IsInPrimaryMainFrame()) {
            return;
          }

          frames.Append(RenderFrameHostToJson(*frame));
        });
    if (!frames.empty()) {
      dict.Set("frames", std::move(frames));
    }
  }

  base::Value::List history;
  content::NavigationController& navigation_controller =
      web_contents.GetController();
  for (int i = 0; i < navigation_controller.GetEntryCount(); ++i) {
    content::NavigationEntry& entry = *navigation_controller.GetEntryAtIndex(i);
    base::Value::Dict json_entry;
    json_entry.Set("url", entry.GetURL().PathForRequest());
    if (!entry.GetReferrer().url.is_empty()) {
      json_entry.Set("referrer", entry.GetReferrer().url.PathForRequest());
    }
    json_entry.Set("transition", PageTransitionGetCoreTransitionString(
                                     entry.GetTransitionType()));
    history.Append(std::move(json_entry));
  }
  dict.Set("history", std::move(history));

  content::EvalJsResult launchParamsResults = content::EvalJs(
      web_contents.GetPrimaryMainFrame(),
      "'launchParamsTargetUrls' in window ? launchParamsTargetUrls : []");
  EXPECT_THAT(launchParamsResults, content::EvalJsResult::IsOk());
  base::Value::List launchParamsTargetUrls = launchParamsResults.ExtractList();
  if (!launchParamsTargetUrls.empty()) {
    for (const base::Value& url : launchParamsTargetUrls) {
      dict.EnsureList("launchParams")
          ->Append(GURL(url.GetString()).PathForRequest());
    }
  }

  return dict;
}

// Serializes the state of all tabs in a particular Browser to a json
// dictionary, including which tab is the currently active tab.
//
// For app browsers, the scope path is added to simplify manual debugging to
// identify cases where a source app window can have an out of scope destination
// url loaded in it.
base::Value::Dict BrowserToJson(const Browser& browser) {
  base::Value::Dict dict = base::Value::Dict().Set(
      "browser_type", BrowserTypeToString(browser.type()));
  if (browser.type() == Browser::Type::TYPE_APP ||
      browser.type() == Browser::Type::TYPE_APP_POPUP) {
    CHECK(browser.app_controller());
    const webapps::AppId& app_id = browser.app_controller()->app_id();
    CHECK(!app_id.empty());
    WebAppProvider* provider = WebAppProvider::GetForTest(browser.profile());
    const GURL& app_scope = provider->registrar_unsafe().GetAppScope(app_id);
    if (app_scope.is_valid()) {
      dict.Set("app_scope", app_scope.PathForRequest());
    }
  }
  base::Value::List tabs;
  const TabStripModel* tab_model = browser.tab_strip_model();
  for (int i = 0; i < tab_model->count(); ++i) {
    content::WebContents* const current_contents =
        tab_model->GetWebContentsAt(i);
    // Skip web contents that are being destroyed from showing up in the
    // expectations to prevent flakiness. `WebContentsToJson()` evaluates JS
    // code inside the web contents, which can be flaky if not taken care of.
    if (current_contents->IsBeingDestroyed()) {
      continue;
    }
    base::Value::Dict tab = WebContentsToJson(browser, *current_contents);
    if (i == tab_model->active_index()) {
      tab.Set("active", true);
    }
    tabs.Append(std::move(tab));
  }
  dict.Set("tabs", std::move(tabs));
  return dict;
}

// This helper class monitors WebContents creation in all tabs (of all browsers)
// and can be queried for the last one seen.
class WebContentsCreationMonitor : public ui_test_utils::AllTabsObserver {
 public:
  WebContentsCreationMonitor() { AddAllBrowsers(); }

  content::WebContents* GetLastSeenWebContentsAndStopMonitoring() {
    ConditionMet();
    return last_seen_web_contents_.get();
  }

 private:
  // AllTabsObserver override:
  std::unique_ptr<base::CheckedObserver> ProcessOneContents(
      content::WebContents* web_contents) override {
    last_seen_web_contents_ = web_contents->GetWeakPtr();
    return nullptr;
  }

  base::WeakPtr<content::WebContents> last_seen_web_contents_;
};

bool IsElementInPage(content::RenderFrameHost* host,
                     const std::string& element_id) {
  return content::EvalJs(host, base::StrCat({"document.getElementById('",
                                             element_id, "') != undefined"}))
      .ExtractBool();
}

// IMPORTANT NOTE TO GARDENERS:
//
// TL;DR: Need to disable a specific test? Scroll down and add its name to
// the appropriate OS block below (and include a bug reference).
//
// More detailed version:
//
// To disable a test that is failing, please refer to the following steps:
// 1. Find the full name of the test. The test name should follow the format:
// `TestBaseName/TestSuite.TestClass/TestParams`, the name should be available
// on the trybot failure page itself.
// 2. Add the `TestParams` under BUILDFLAGs inside the `disabled_flaky_tests`
// set below, to ensure that a single test is only disabled for the OS or builds
// it is flaking on.
// 3. Add the appropriate TODO with a public bug so that the flaky tests can be
// tracked.
//
// Once flakiness has been fixed, please remove the entry from here so that test
// suites can start running the test again.
static const base::flat_set<std::string> disabled_flaky_tests = {
#if defined(ADDRESS_SANITIZER)
    // TODO(crbug.com/377425233): Fix flakiness on ASAN.
    "AppAStandaloneAppBBrowser_CaptureOn_AppWnd_ScopeA2B_ServerSideViaB_"
    "ViaLink_MiddleClick_WithoutOpener_TargetBlank",
    "AppANavigateExistingAppBFocusExisting_BothStandalone_CaptureOn_AppWnd_"
    "ScopeA2B_ServerSideViaB_ViaLink_LeftClick_WithoutOpener_TargetBlank",
    "AppANavigateExistingAppBFocusExisting_BothStandalone_CaptureOn_AppWnd_"
    "ScopeA2B_ServerSideViaX_ViaLink_LeftClick_WithoutOpener_TargetBlank",
    "FocusExisting_BothStandalone_CaptureOn_AppWnd_ScopeA2B_Direct_ViaLink_"
    "RightClick_WithoutOpener_TargetBlank",
    "FocusExisting_BothStandalone_CaptureOn_Tab_ScopeA2B_Direct_ViaLink_"
    "RightClick_WithoutOpener_TargetBlank",
    "NavigateExisting_BothStandalone_CaptureOn_AppWnd_ScopeA2B_ServerSideViaA_"
    "ViaLink_LeftClick_WithoutOpener_TargetBlank",
    "NavigateExisting_BothStandalone_CaptureOn_AppWnd_ScopeA2B_ServerSideViaX_"
    "ViaLink_LeftClick_WithoutOpener_TargetBlank",
    "NavigateExisting_BothStandalone_CaptureOn_Tab_ScopeA2B_ServerSideViaA_"
    "ViaLink_LeftClick_WithoutOpener_TargetBlank",
    "NavigateExisting_BothStandalone_CaptureOn_Tab_ScopeA2B_ServerSideViaX_"
    "ViaLink_LeftClick_WithoutOpener_TargetBlank",
    "AppANavigateExistingAppBFocusExisting_BothStandalone_CaptureOn_AppWnd_"
    "ScopeA2B_ServerSideViaX_ViaLink_LeftClick_WithoutOpener_TargetBlank",
    "NavigateExisting_BothStandalone_CaptureOn_Tab_ScopeA2B_ServerSideViaX_"
    "ViaLink_LeftClick_WithoutOpener_TargetBlank",
#endif
#if BUILDFLAG(IS_MAC)
    // TODO(crbug.com/372119276): Fix flakiness for `Redirection_OpenInChrome`
    // tests on MacOS.
    "BothStandalone_CaptureOn_AppWnd_ScopeA2X_ServerSideViaB_ViaLink_"
    "ShiftClick_WithOpener_TargetBlank",
    "BothStandalone_CaptureOn_AppWnd_ScopeA2X_ServerSideViaA_ViaLink_"
    "ShiftClick_WithOpener_TargetBlank",
    "BothStandalone_CaptureOn_AppWnd_ScopeA2X_ServerSideViaA_ViaLink_"
    "MiddleClick_WithOpener_TargetBlank",
#elif BUILDFLAG(IS_LINUX)
#elif BUILDFLAG(IS_WIN)
#elif BUILDFLAG(IS_CHROMEOS)
    // TODO(crbug.com/359600606): Fix failures of AppBBrowser/BothBrowser tests
    // on ChromeOS.
    "AppAStandaloneAppBBrowser_CaptureOn_AppWnd_ScopeA2B_Direct_ViaButton_"
    "LeftClick_WithoutOpener_TargetBlank",
    "AppAStandaloneAppBBrowser_CaptureOn_AppWnd_ScopeA2B_Direct_ViaButton_"
    "MiddleClick_WithoutOpener_TargetBlank",
    "AppAStandaloneAppBBrowser_CaptureOn_AppWnd_ScopeA2B_Direct_ViaButton_"
    "ShiftClick_WithoutOpener_TargetBlank",
    "AppAStandaloneAppBBrowser_CaptureOn_AppWnd_ScopeA2B_Direct_ViaLink_"
    "LeftClick_WithoutOpener_TargetBlank",
    "AppAStandaloneAppBBrowser_CaptureOn_AppWnd_ScopeA2B_Direct_ViaLink_"
    "MiddleClick_WithOpener_TargetBlank",
    "AppAStandaloneAppBBrowser_CaptureOn_AppWnd_ScopeA2B_Direct_ViaLink_"
    "MiddleClick_WithoutOpener_TargetBlank",
    "AppAStandaloneAppBBrowser_CaptureOn_AppWnd_ScopeA2B_Direct_ViaLink_"
    "ShiftClick_WithOpener_TargetBlank",
    "AppAStandaloneAppBBrowser_CaptureOn_AppWnd_ScopeA2B_Direct_ViaLink_"
    "ShiftClick_WithoutOpener_TargetBlank",
    "AppAStandaloneAppBBrowser_CaptureOn_AppWnd_ScopeA2B_ServerSideViaA_"
    "ViaLink_MiddleClick_WithoutOpener_TargetBlank",
    "AppAStandaloneAppBBrowser_CaptureOn_AppWnd_ScopeA2B_ServerSideViaA_"
    "ViaLink_ShiftClick_WithoutOpener_TargetBlank",
    "AppAStandaloneAppBBrowser_CaptureOn_AppWnd_ScopeA2B_ServerSideViaB_"
    "ViaLink_MiddleClick_WithoutOpener_TargetBlank",
    "AppAStandaloneAppBBrowser_CaptureOn_AppWnd_ScopeA2B_ServerSideViaB_"
    "ViaLink_ShiftClick_WithoutOpener_TargetBlank",
    "AppAStandaloneAppBBrowser_CaptureOn_AppWnd_ScopeA2B_ServerSideViaX_"
    "ViaLink_ShiftClick_WithoutOpener_TargetBlank",
    "AppAStandaloneAppBBrowser_CaptureOn_Tab_ScopeA2B_Direct_ViaButton_"
    "LeftClick_WithoutOpener_TargetBlank",
    "AppAStandaloneAppBBrowser_CaptureOn_Tab_ScopeA2B_Direct_ViaButton_"
    "MiddleClick_WithoutOpener_TargetBlank",
    "AppAStandaloneAppBBrowser_CaptureOn_Tab_ScopeA2B_Direct_ViaButton_"
    "ShiftClick_WithoutOpener_TargetBlank",
    "AppAStandaloneAppBBrowser_CaptureOn_Tab_ScopeA2B_Direct_ViaLink_LeftClick_"
    "WithoutOpener_TargetBlank",
    "AppAStandaloneAppBBrowser_CaptureOn_Tab_ScopeA2B_Direct_ViaLink_"
    "MiddleClick_WithOpener_TargetBlank",
    "AppAStandaloneAppBBrowser_CaptureOn_Tab_ScopeA2B_Direct_ViaLink_"
    "MiddleClick_WithoutOpener_TargetBlank",
    "AppAStandaloneAppBBrowser_CaptureOn_Tab_ScopeA2B_Direct_ViaLink_"
    "ShiftClick_WithOpener_TargetBlank",
    "AppAStandaloneAppBBrowser_CaptureOn_Tab_ScopeA2B_Direct_ViaLink_"
    "ShiftClick_WithoutOpener_TargetBlank",
    "AppAStandaloneAppBBrowser_CaptureOn_Tab_ScopeA2B_ServerSideViaA_ViaLink_"
    "LeftClick_WithoutOpener_TargetBlank",
    "AppAStandaloneAppBBrowser_CaptureOn_Tab_ScopeA2B_ServerSideViaB_ViaLink_"
    "LeftClick_WithoutOpener_TargetBlank",
    "AppAStandaloneAppBBrowser_CaptureOn_Tab_ScopeA2B_ServerSideViaB_ViaLink_"
    "MiddleClick_WithoutOpener_TargetBlank",
    "AppAStandaloneAppBBrowser_CaptureOn_Tab_ScopeA2B_ServerSideViaB_ViaLink_"
    "ShiftClick_WithoutOpener_TargetBlank",
    "AppAStandaloneAppBBrowser_CaptureOn_Tab_ScopeA2B_ServerSideViaX_ViaLink_"
    "LeftClick_WithoutOpener_TargetBlank",
    "BothBrowser_CaptureOn_Tab_ScopeA2A_Direct_ViaLink_LeftClick_WithoutOpener_"
    "TargetBlank",
    "BothBrowser_CaptureOn_Tab_ScopeA2B_Direct_ViaLink_LeftClick_WithoutOpener_"
    "TargetBlank",
    "FocusExisting_AppAStandaloneAppBBrowser_CaptureOn_AppWnd_ScopeA2B_Direct_"
    "ViaLink_LeftClick_WithoutOpener_TargetBlank",
    "FocusExisting_AppAStandaloneAppBBrowser_CaptureOn_AppWnd_ScopeA2B_Direct_"
    "ViaLink_LeftClick_WithoutOpener_TargetBlank",
    "FocusExisting_AppAStandaloneAppBBrowser_CaptureOn_Tab_ScopeA2B_Direct_"
    "ViaLink_LeftClick_WithoutOpener_TargetBlank",
    "FocusExisting_AppAStandaloneAppBBrowser_CaptureOn_Tab_ScopeA2B_Direct_"
    "ViaLink_LeftClick_WithoutOpener_TargetBlank",
    "FocusExisting_AppAStandaloneAppBBrowser_CaptureOn_Tab_ScopeA2B_"
    "ServerSideViaA_ViaLink_LeftClick_WithoutOpener_TargetBlank",
    "FocusExisting_BothBrowser_CaptureOn_Tab_ScopeA2A_Direct_ViaLink_LeftClick_"
    "WithoutOpener_TargetBlank",
    "FocusExisting_BothBrowser_CaptureOn_Tab_ScopeA2B_Direct_ViaLink_LeftClick_"
    "WithoutOpener_TargetBlank",
    "FocusExisting_BothBrowser_CaptureOn_Tab_ScopeA2B_ServerSideViaA_ViaLink_"
    "LeftClick_WithoutOpener_TargetBlank",
    "NavigateExisting_AppAStandaloneAppBBrowser_CaptureOn_AppWnd_ScopeA2B_"
    "Direct_ViaLink_LeftClick_WithoutOpener_TargetBlank",
    "NavigateExisting_AppAStandaloneAppBBrowser_CaptureOn_Tab_ScopeA2B_Direct_"
    "ViaLink_LeftClick_WithoutOpener_TargetBlank",
    "NavigateExisting_BothBrowser_CaptureOn_Tab_ScopeA2A_Direct_ViaLink_"
    "LeftClick_WithoutOpener_TargetBlank",
    "NavigateExisting_BothBrowser_CaptureOn_Tab_ScopeA2B_Direct_ViaLink_"
    "LeftClick_WithoutOpener_TargetBlank",
    "NavigateExisting_AppAStandaloneAppBBrowser_CaptureOn_Tab_ScopeA2B_"
    "ServerSideViaA_ViaLink_LeftClick_WithoutOpener_TargetBlank",
    "NavigateExisting_BothBrowser_CaptureOn_Tab_ScopeA2B_ServerSideViaA_"
    "ViaLink_LeftClick_WithoutOpener_TargetBlank",
#endif
};

// This test verifies the navigation capture logic by testing by launching sites
// inside app containers and tabs and test what happens when links are
// left/middle clicked and window.open is used (whether browser objects are
// reused and what type gets launched).
//
// The test expectations are read from json files that are stored here.
// The main test expectations files:
// chrome/test/data/web_apps/navigation_capture_expectations*.json
// Secondary: For tests that expect App B to be launched when the test starts.
// chrome/test/data/web_apps/navigation_capture_test_launch_app_b.json
//
// The expectations files map test names (as serialized from the test
// parameters) to a json object containing a `disabled` flag as well as
// `expected_state`, the expected state of all Browser objects and their
// WebContents at the end of a test.
//
// If link capturing behavior changes, the test expectations would need to be
// updated. This can be done manually (by editing the json file directly), or it
// can be done automatically by using the flag --rebaseline-link-capturing-test.
//
// By default only tests that aren't listed as disabled in the json file are
// executed. To also run tests marked as disabled, include the --run-all-tests
// flag. This is also needed if you want to rebaseline tests that are still
// disabled.
//
// Example usage:
// out/Default/browser_tests \
// --gtest_filter=*WebAppLinkCapturingParameterizedBrowserTest.* \
// --rebaseline-link-capturing-test --run-all-tests --test-launcher-jobs=40
class WebAppLinkCapturingParameterizedBrowserTest
    : public WebAppBrowserTestBase,
      public testing::WithParamInterface<LinkCaptureTestParam> {
 public:
  WebAppLinkCapturingParameterizedBrowserTest() {
    // kDropInputEventsBeforeFirstPaint is disabled to de-flake our simulated
    // clicks.
    std::string mode = "reimpl_default_on";
    const char* param_name =
        ::testing::UnitTest::GetInstance()->current_test_info()->value_param();
    if (param_name != nullptr && std::string_view(param_name).length() > 0) {
      // GetParam() crashes unless this test is run as a parameterized test. The
      // 'Cleanup' tests are not.
      if (GetLinkCapturing() == LinkCapturing::kEnabledViaClientMode) {
        mode = "reimpl_on_via_client_mode";
      }
    }
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{base::test::FeatureRefAndParams(
            features::kPwaNavigationCapturing,
            {{"link_capturing_state", mode}})},
        /*disabled_features=*/{
            blink::features::kDropInputEventsBeforeFirstPaint});
  }

  // Returns the expectations JSON file name without extension.
  virtual std::string GetExpectationsFileBaseName() const {
    return "navigation_capture_expectations";
  }

  // This function allows derived test suites to configure custom
  // pre-condition steps before each test runs.
  //
  // @param app_a The id of an app A (aka. 'source app').
  // @param app_b The id of an app B (aka. a possible 'destination app').
  virtual void MaybeCustomSetup(const webapps::AppId& app_a,
                                const webapps::AppId& app_b) {}

  virtual std::string GetTestClassName() const {
    return "WebAppLinkCapturingParameterizedBrowserTest";
  }

  // Listens for a DomMessage that starts with FinishedNavigating.
  //
  // @param message_queue The message queue expected to see the message.
  void WaitForNavigationFinishedMessage(
      content::DOMMessageQueue& message_queue) {
    std::string message;
    EXPECT_TRUE(message_queue.WaitForMessage(&message));
    std::string unquoted_message;
    ASSERT_TRUE(base::RemoveChars(message, "\"", &unquoted_message)) << message;
    EXPECT_TRUE(base::StartsWith(unquoted_message, "FinishedNavigating"))
        << unquoted_message;
    DLOG(INFO) << message;
  }

  // The expectations file can depend on whether link capturing is enabled or
  // not (and likely more things in the future).
  base::FilePath GetExpectationsFile(ExpectationsFileConfig file_config) const {
    std::string filename =
        GetExpectationsFileBaseName() + GetExpectationsFileSuffix(file_config);
    return base::PathService::CheckedGet(base::DIR_SRC_TEST_DATA_ROOT)
        .AppendASCII(kLinkCaptureTestInputPathPrefix)
        .AppendASCII(filename)
        .AddExtensionASCII("json");
  }

  std::unique_ptr<net::test_server::HttpResponse> SimulateRedirectHandler(
      const net::test_server::HttpRequest& request) {
    if (GetRedirectType() == RedirectType::kNone) {
      return nullptr;  // This test is not using redirects.
    }

    // The way the tests are currently set up, there should only be a single
    // redirection happening on the way from a source to a destination url.
    // Prevent multiple redirections from being triggered which causes a Chrome
    // error page to show up, cancelling the navigation.
    if (did_redirect_) {
      return nullptr;
    }

    // Strip out queries and fragments from the request url, since the id and
    // click type is appended by the test file to the url on click events for
    // debugging, which interferes with the redirection logic.
    GURL::Replacements request_replacements;
    request_replacements.ClearRef();
    request_replacements.ClearQuery();
    const GURL& final_request_url =
        request.GetURL().ReplaceComponents(request_replacements);

    if (!base::Contains(final_request_url.spec(), "/destination.html")) {
      return nullptr;  // Only redirect for destination pages.
    }

    GURL redirect_from = GetRedirectIntermediateUrl();
    GURL redirect_to = GetDestinationUrl();

    // We don't redirect requests for start.html, manifest files, etc. Only the
    // destination page the test wants to run.
    if (final_request_url != redirect_from) {
      return nullptr;
    }

    // Repopulate queries and fragments from the request url into the
    // destination url.
    GURL::Replacements destination_replacements;
    GURL request_url = request.GetURL();
    destination_replacements.SetRefStr(request_url.ref_piece());
    destination_replacements.SetQueryStr(request_url.query_piece());
    redirect_to = redirect_to.ReplaceComponents(destination_replacements);

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_TEMPORARY_REDIRECT);
    response->set_content_type("text/html");
    response->AddCustomHeader("Location", redirect_to.spec());
    response->AddCustomHeader("Access-Control-Allow-Origin", "*");
    response->set_content(base::StringPrintf(
        "<!doctype html><p>Redirecting to %s", redirect_to.spec().c_str()));

    did_redirect_ = true;
    return response;
  }

 protected:
  void EnsureValidNewTabPage() {
    // Ensure that if a fixture ended up loading a different page in the
    // starting tab, create a new tab for the navigation.
    GURL last_committed_url = browser()
                                  ->tab_strip_model()
                                  ->GetActiveWebContents()
                                  ->GetLastCommittedURL();
    bool is_at_new_tab_page =
        IsNewTabOrAboutBlankUrl(browser(), last_committed_url);
    if (!is_at_new_tab_page) {
      LOG(ERROR) << "opening new tab due to "
                 << last_committed_url.possibly_invalid_spec();
      chrome::NewTab(browser());
    }
  }

  content::WebContents* LaunchStartPageAsApp(const webapps::AppId& app_id,
                                             const GURL& url) {
    base::test::TestFuture<base::WeakPtr<Browser>,
                           base::WeakPtr<content::WebContents>,
                           apps::LaunchContainer>
        launch_future;

    content::DOMMessageQueue message_queue;
    provider().scheduler().LaunchApp(app_id, url, launch_future.GetCallback());
    EXPECT_TRUE(launch_future.Wait());
    content::WebContents* contents =
        launch_future.Get<base::WeakPtr<content::WebContents>>().get();
    content::WaitForLoadStop(contents);
    WaitForNavigationFinishedMessage(message_queue);
    return contents;
  }

  content::WebContents* LaunchPageInTab(const GURL& url) {
    content::DOMMessageQueue message_queue;
    // Note: We do not need to call WaitForLoadStop because NavigateToURL calls
    // that internally.
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    WaitForNavigationFinishedMessage(message_queue);
    return contents;
  }

  void ClickIntentPickerChip(Browser* browser) {
    ui_test_utils::BrowserChangeObserver app_browser_observer(
        nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
    // Clicking the Intent Picker will trigger a re-parenting (not a new
    // navigation, so the DomMessage has already been sent).
    ASSERT_TRUE(web_app::ClickIntentPickerChip(browser));
    app_browser_observer.Wait();

    // After re-parenting, the old browser gets a new tab contents and we
    // need to wait for that to finish loading before capturing the end
    // state.
    WaitForLoadStop(browser->tab_strip_model()->GetActiveWebContents());
  }

  void GetNewContentsAndPropagationOfLaunchParams(
      WebContentsCreationMonitor& monitor) {
    content::WebContents* handled_contents =
        monitor.GetLastSeenWebContentsAndStopMonitoring();

    // Some navigations might cause the handled_contents to be closed (for
    // e.g, capturable redirections ending in an app with focus-existing).
    if (handled_contents) {
      content::WaitForLoadStop(handled_contents);
      ASSERT_NE(nullptr, handled_contents);
      ASSERT_TRUE(handled_contents->GetURL().is_valid());
    }

    provider().command_manager().AwaitAllCommandsCompleteForTesting();
    // Attempt to ensure that all launchParams have propagated.
    content::RunAllTasksUntilIdle();
  }

  // Prevent the creation of obviously invalid test expectation during
  // re-baselining.
  virtual void AssertValidTestConfiguration() {
    // For the Intent Picker, only one combination makes sense:
    if (GetNavigationElement() == NavigationElement::kElementIntentPicker) {
      ASSERT_EQ(LinkCapturing::kEnabled, GetLinkCapturing());
      ASSERT_EQ(StartingPoint::kTab, GetStartingPoint());
      ASSERT_EQ(Destination::kScopeA2A, GetDestination());
      ASSERT_EQ(RedirectType::kNone, GetRedirectType());
      ASSERT_EQ(test::ClickMethod::kLeftClick, ClickMethod());
      ASSERT_EQ(OpenerMode::kNoOpener, GetOpenerMode());
      ASSERT_EQ(NavigationTarget::kNoFrame, GetNavigationTarget());
      // At the moment, only kAuto is tested, but it is conceivable we'd add
      // others. For kNavigateExisting, see the comment regarding
      // `expect_navigation` below before enabling.
      ASSERT_EQ(ClientModeCombination::kAuto, GetClientModeCombination());
    }

    if (GetNavigationElement() ==
        NavigationElement::kElementServiceWorkerButton) {
      ASSERT_EQ(test::ClickMethod::kLeftClick, ClickMethod());
      ASSERT_EQ(OpenerMode::kNoOpener, GetOpenerMode());
      ASSERT_EQ(NavigationTarget::kBlank, GetNavigationTarget());
    }

    // For right clicks, redirect URL should not be kServerSideViaX because
    // redirection does not happen via X (uninstalled app).
    if (ClickMethod() == test::ClickMethod::kRightClickLaunchApp) {
      ASSERT_NE(RedirectType::kServerSideViaX, GetRedirectType());
      // This is for right click use cases in redirection. Note that this
      // does not apply if there is no redirection happening.
      if (GetRedirectType() != RedirectType::kNone) {
        ASSERT_NE(Destination::kScopeA2X, GetDestination());
        ASSERT_EQ(OpenerMode::kNoOpener, GetOpenerMode());
        ASSERT_EQ(NavigationElement::kElementLink, GetNavigationElement());
        ASSERT_EQ(NavigationTarget::kBlank, GetNavigationTarget());
      }
    }
  }

  // Trigger a right click on an HTML element, wait for the context menu to
  // show up and mimic an `Open link in <Installed App>` flow.
  void SimulateRightClickOnElementAndLaunchApp(content::WebContents* contents,
                                               const std::string& element_id) {
    base::test::TestFuture<RenderViewContextMenu*> future_callback;
    ContextMenuNotificationObserver context_menu_observer(
        IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP, /*event_flags=*/0,
        future_callback.GetCallback());
    test::SimulateClickOnElement(contents, element_id,
                                 test::ClickMethod::kRightClickLaunchApp);
    ASSERT_TRUE(future_callback.Wait());
  }

  // The json file is of the following format:
  // { 'tests': {
  //   'Shortened_Test_Name': {
  //      "_params": "Full_Test_Params",
  //      "disabled": <false if bots should fail when Expectations don't match>,
  //      "expected_state": {<expected state of all browsers/apps>}
  //    }
  //    ...
  // }}
  // This method returns the dictionary associated with the test name derived
  // from the test parameters. If no entry exists for the test, a new one is
  // created.
  base::Value::Dict& GetTestCaseDataFromParam() {
    std::string shortened_name =
        TupleToParamString(GetShortenedConfigFromTestConfig(GetParam()));
    base::Value::Dict* result =
        test_expectations().EnsureDict("tests")->EnsureDict(shortened_name);
    // Temporarily check expectations for the test name before redirect mode was
    // a separate parameter as well to make it easier to migrate expectations.
    // TODO(mek): Remove this migration code.
    if (!result->contains("expected_state") &&
        GetRedirectType() == RedirectType::kNone) {
      base::ReplaceFirstSubstringAfterOffset(&shortened_name, 0, "_Direct", "");
      *result = test_expectations()
                    .EnsureDict("tests")
                    ->EnsureDict(shortened_name)
                    ->Clone();
      test_expectations().EnsureDict("tests")->Remove(shortened_name);
    }
    return *result;
  }

  base::ScopedClosureRunner LockExpectationsFile(
      ExpectationsFileConfig file_config) {
    CHECK(ShouldRebaseline());

    base::FilePath lock_file_path =
        base::PathService::CheckedGet(base::DIR_OUT_TEST_DATA_ROOT)
            .Append(GetExpectationsFile(file_config).BaseName())
            .AddExtensionASCII("lock");

    // Lock the results file to support using `--test-launcher-jobs=X` when
    // doing a rebaseline.
    base::File exclusive_file = base::File(
        lock_file_path, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_WRITE);

// Fuchsia doesn't support file locking.
#if !BUILDFLAG(IS_FUCHSIA)
    {
      SCOPED_TRACE("Attempting to gain exclusive lock of " +
                   lock_file_path.MaybeAsASCII());
      base::test::RunUntil([&]() {
        return exclusive_file.Lock(base::File::LockMode::kExclusive) ==
               base::File::FILE_OK;
      });
    }
#endif  // !BUILDFLAG(IS_FUCHSIA)

    // Re-read expectations to catch changes from other parallel runs of
    // rebaselining.
    InitializeTestExpectations(file_config);

    return base::ScopedClosureRunner(base::BindOnce(
        [](base::File lock_file) {
#if !BUILDFLAG(IS_FUCHSIA)
          EXPECT_EQ(lock_file.Unlock(), base::File::FILE_OK);
#endif  // !BUILDFLAG(IS_FUCHSIA)
          lock_file.Close();
        },
        std::move(exclusive_file)));
  }

  // Serializes the entire state of chrome that we're interested in in this test
  // to a dictionary. This state consists of the state of all Browser windows,
  // in creation order of the Browser.
  base::Value::Dict CaptureCurrentState() {
    base::Value::List browsers;
    for (Browser* b : *BrowserList::GetInstance()) {
      if (b->is_delete_scheduled()) {
        continue;
      }
      base::Value::Dict json_browser = BrowserToJson(*b);
      browsers.Append(std::move(json_browser));
    }

    // Checks whether the web app launch metrics have been measured for the
    // current navigation.
    std::vector<base::Bucket> buckets =
        action_histogram_tester_->GetAllSamples("WebApp.LaunchSource");
    base::Value::List bucket_list;
    for (const base::Bucket& bucket : buckets) {
      for (int count = 0; count < bucket.count; count++) {
        bucket_list.Append(
            base::ToString(static_cast<apps::LaunchSource>(bucket.min)));
      }
    }

    return base::Value::Dict()
        .Set("browsers", std::move(browsers))
        .Set("launch_metric_buckets", std::move(bucket_list));
  }

  // This function is used during rebaselining to record (to a file) the results
  // from an actual run of a single test case, used by developers to update the
  // expectations. Constructs a json dictionary and saves it to the test results
  // json file. Returns true if writing was successful.
  void RecordActualResults(ExpectationsFileConfig file_config) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    // Lock the results file to support using `--test-launcher-jobs=X` when
    // doing a rebaseline.
    base::ScopedClosureRunner lock = LockExpectationsFile(file_config);

    base::Value::Dict& test_case_to_be_updated = GetTestCaseDataFromParam();
    base::Value::Dict saved_test_case = test_case_to_be_updated.Clone();

    std::string full_test_params = TupleToParamString(GetParam());
    test_case_to_be_updated.Set("_params", full_test_params);
    // If this is a new test case, start it out as disabled until we've manually
    // verified the expectations are correct.
    if (!test_case_to_be_updated.contains("expected_state")) {
      test_case_to_be_updated.Set("disabled", true);
    }
    test_case_to_be_updated.Set("expected_state", CaptureCurrentState());
    if (saved_test_case == test_case_to_be_updated) {
      // This prevents file save churn when rebaselining, to reduce flakiness
      // when reading the file on test initialization.
      LOG(INFO) << "No changes detected for test case " << full_test_params
                << ", not saving file "
                << GetExpectationsFile(file_config).value();
    } else {
      SaveExpectations(file_config);
    }
  }

  void SaveExpectations(ExpectationsFileConfig file_config) {
    CHECK(ShouldRebaseline());
    // Write formatted JSON back to disk.
    std::optional<std::string> json_string = base::WriteJsonWithOptions(
        *test_expectations_, base::JsonOptions::OPTIONS_PRETTY_PRINT);
    ASSERT_TRUE(json_string.has_value());
    ASSERT_TRUE(
        base::WriteFile(GetExpectationsFile(file_config), *json_string));
  }

  LinkCapturing GetLinkCapturing() const {
    return std::get<LinkCapturing>(GetParam());
  }

  AppUserDisplayMode GetAppUserDisplayMode() const {
    return std::get<AppUserDisplayMode>(GetParam());
  }

  mojom::UserDisplayMode GetUserDisplayMode(GURL start_url) const {
    CHECK(start_url == GetDestinationUrlPageA() ||
          start_url == GetDestinationUrlPageB());
    switch (GetAppUserDisplayMode()) {
      case AppUserDisplayMode::kBothBrowser:
        return mojom::UserDisplayMode::kBrowser;
      case AppUserDisplayMode::kBothStandalone:
        return mojom::UserDisplayMode::kStandalone;
      case AppUserDisplayMode::kAppAStandaloneAppBBrowser:
        return start_url == GetDestinationUrlPageA()
                   ? mojom::UserDisplayMode::kStandalone
                   : mojom::UserDisplayMode::kBrowser;
    }
  }

  ClientModeCombination GetClientModeCombination() const {
    return std::get<ClientModeCombination>(GetParam());
  }

  StartingPoint GetStartingPoint() const {
    return std::get<StartingPoint>(GetParam());
  }

  // Returns `true` if the test should start inside an app window (and `false`
  // if the test should start in a tab).
  bool StartInAppWindow() const {
    return GetStartingPoint() == StartingPoint::kAppWindow;
  }

  Destination GetDestination() const {
    return std::get<Destination>(GetParam());
  }

  GURL GetDestinationUrlPageA() const {
    return embedded_test_server()->GetURL(kDestinationPageScopeA);
  }

  GURL GetDestinationUrlPageB() const {
    return embedded_test_server()->GetURL(kDestinationPageScopeB);
  }

  GURL GetDestinationUrlPageX() const {
    return embedded_test_server()->GetURL(kDestinationPageScopeX);
  }

  GURL GetDestinationUrl() const {
    switch (GetDestination()) {
      case Destination::kScopeA2A:
        return GetDestinationUrlPageA();
      case Destination::kScopeA2B:
        return GetDestinationUrlPageB();
      case Destination::kScopeA2X:
        return GetDestinationUrlPageX();
    }
  }

  RedirectType GetRedirectType() const {
    return std::get<RedirectType>(GetParam());
  }

  GURL GetRedirectIntermediateUrl() {
    switch (GetRedirectType()) {
      case RedirectType::kNone:
        return GURL();
      case RedirectType::kServerSideViaA:
        return embedded_test_server()->GetURL(kDestinationPageScopeA);
      case RedirectType::kServerSideViaB:
        return embedded_test_server()->GetURL(kDestinationPageScopeB);
      case RedirectType::kServerSideViaX:
        return embedded_test_server()->GetURL(kDestinationPageScopeX);
    }
  }

  NavigationElement GetNavigationElement() const {
    return std::get<NavigationElement>(GetParam());
  }

  test::ClickMethod ClickMethod() const {
    return std::get<test::ClickMethod>(GetParam());
  }

  OpenerMode GetOpenerMode() const { return std::get<OpenerMode>(GetParam()); }

  // Returns `true` if the test should supply an opener value.
  bool WithOpener() const { return GetOpenerMode() == OpenerMode::kOpener; }

  NavigationTarget GetNavigationTarget() const {
    return std::get<NavigationTarget>(GetParam());
  }

  // The test page contains elements (links and buttons) that are configured
  // for each combination. This function obtains the right element id to use
  // in the navigation click.
  std::string GetElementId() const {
    return base::JoinString(
        {"id", ToIdString(GetNavigationElement()),
         ToIdString(GetRedirectType(), GetDestination()),
         ToIdString(GetNavigationTarget()), ToIdString(GetOpenerMode())},
        "-");
  }

  webapps::AppId InstallTestWebApp(
      const GURL& start_url,
      blink::mojom::ManifestLaunchHandler_ClientMode client_mode) {
    auto web_app_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->launch_handler = blink::Manifest::LaunchHandler(client_mode);
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->display_mode = blink::mojom::DisplayMode::kStandalone;
    web_app_info->user_display_mode = GetUserDisplayMode(start_url);
    const webapps::AppId app_id =
        test::InstallWebApp(profile(), std::move(web_app_info));
    apps::AppReadinessWaiter(profile(), app_id).Await();
    return app_id;
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebAppBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  // Returns true if re-baseline was signalled, via a command line switch.
  bool ShouldRebaseline() {
    const base::CommandLine& command_line =
        *base::CommandLine::ForCurrentProcess();
    return command_line.HasSwitch("rebaseline-link-capturing-test");
  }

  bool ShouldRunDisabledTests() {
    const base::CommandLine& command_line =
        *base::CommandLine::ForCurrentProcess();
    return command_line.HasSwitch("run-all-tests");
  }

  Profile* profile() { return browser()->profile(); }

  void SetUpOnMainThread() override {
    WebAppBrowserTestBase::SetUpOnMainThread();

    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &WebAppLinkCapturingParameterizedBrowserTest::SimulateRedirectHandler,
        base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());

    NotificationPermissionContext::UpdatePermission(
        browser()->profile(), embedded_test_server()->GetOrigin().GetURL(),
        CONTENT_SETTING_ALLOW);
    notification_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile());
    notification_tester_->SetNotificationAddedClosure(
        base::BindLambdaForTesting([this] {
          std::vector<message_center::Notification> notifications =
              notification_tester_->GetDisplayedNotificationsForType(
                  NotificationHandler::Type::WEB_PERSISTENT);
          EXPECT_EQ(1ul, notifications.size());
          for (const message_center::Notification& notification :
               notifications) {
            notification_tester_->SimulateClick(
                NotificationHandler::Type::WEB_PERSISTENT, notification.id(),
                /*action_index=*/std::nullopt, /*reply=*/std::nullopt);
          }
        }));
  }

  // This test verifies that there are no left-over expectations for tests that
  // no longer exist in code but still exist in the expectations json file.
  // Additionally if this test is run with the --rebaseline-link-capturing-test
  // flag any left-over expectations will be cleaned up.
  void PerformTestCleanupIfNeeded(const ExpectationsFileConfig& file_config) {
    InitializeTestExpectations(file_config);

    // Iterate over all the tests in all the test suites (even unrelated ones)
    // to obtain a list of the test cases that belong to our test class.
    std::set<std::string> shortened_test_cases;
    const testing::UnitTest* unit_test = testing::UnitTest::GetInstance();
    for (int i = 0; i < unit_test->total_test_suite_count(); ++i) {
      const testing::TestSuite* test_suite = unit_test->GetTestSuite(i);
      // We only care about link capturing parameterized tests.
      if (!base::Contains(std::string(test_suite->name()),
                          GetTestClassName())) {
        continue;
      }
      for (int j = 0; j < test_suite->total_test_count(); ++j) {
        const char* test_name = test_suite->GetTestInfo(j)->name();
        auto parts = base::SplitStringOnce(test_name, '/');
        if (!parts.has_value()) {
          // Not a parameterized test.
          continue;
        }
        std::string_view full_test_params = parts->second;
        // Only include the test as a candidate test for this file if the
        // current test is considered within the test file configuration.
        if (!DoesTestMatchFileConfig(full_test_params, file_config)) {
          continue;
        }
        std::string shortened_name =
            RemoveExpectationsFileConfigFromFullTestParams(full_test_params,
                                                           file_config);
        shortened_test_cases.emplace(std::move(shortened_name));
      }
    }

    base::ScopedAllowBlockingForTesting allow_blocking;
    base::ScopedClosureRunner lock;
    if (ShouldRebaseline()) {
      lock = LockExpectationsFile(file_config);
    }

    base::Value::Dict& expectations = *test_expectations().EnsureDict("tests");
    std::vector<std::string> tests_to_remove;
    for (const auto [shortened_name, _] : expectations) {
      if (!shortened_test_cases.contains(shortened_name)) {
        tests_to_remove.push_back(shortened_name);
      }
    }
    if (ShouldRebaseline()) {
      for (const auto& shortened_name : tests_to_remove) {
        LOG(INFO) << "Removing " << shortened_name;
        expectations.Remove(shortened_name);
      }
      if (file_read_success_ || expectations.size() > 0) {
        SaveExpectations(file_config);
      } else {
        LOG(INFO)
            << "File " << GetExpectationsFile(file_config).value()
            << " didn't exist and will not have tests, so not saving anything.";
      }
    } else {
      EXPECT_THAT(tests_to_remove, testing::ElementsAre())
          << "Run this test with --rebaseline-link-capturing-test to clean "
             "this up.";
    }
  }

  base::Value::Dict& test_expectations() {
    CHECK(test_expectations_.has_value());
    CHECK(test_expectations_->is_dict());
    return test_expectations_->GetDict();
  }

  void RunTest() {
    // Parses the corresponding json file for test expectations given the
    // respective test suite.
    InitializeTestExpectations(
        GetExpectationsFileConfigFromTestConfig(GetParam()));

    if (ShouldSkipCurrentTest()) {
      GTEST_SKIP()
          << "Skipped as test is marked as disabled in the expectations file. "
             "Add the switch '--run-all-tests' to run disabled tests too.";
    }

    AssertValidTestConfiguration();

    DLOG(INFO) << "Installing apps.";

    // Install apps for scope A and B (note: scope X is deliberately excluded)
    // with the correct launch handling client modes defined.

    blink::mojom::ManifestLaunchHandler_ClientMode client_mode_a;
    blink::mojom::ManifestLaunchHandler_ClientMode client_mode_b;
    switch (GetClientModeCombination()) {
      case ClientModeCombination::kAuto:
        client_mode_a = blink::mojom::ManifestLaunchHandler_ClientMode::kAuto;
        client_mode_b = blink::mojom::ManifestLaunchHandler_ClientMode::kAuto;
        break;
      case ClientModeCombination::kBothNavigateNew:
        client_mode_a =
            blink::mojom::ManifestLaunchHandler_ClientMode::kNavigateNew;
        client_mode_b =
            blink::mojom::ManifestLaunchHandler_ClientMode::kNavigateNew;
        break;
      case ClientModeCombination::kBothNavigateExisting:
        client_mode_a =
            blink::mojom::ManifestLaunchHandler_ClientMode::kNavigateExisting;
        client_mode_b =
            blink::mojom::ManifestLaunchHandler_ClientMode::kNavigateExisting;
        break;
      case ClientModeCombination::kBothFocusExisting:
        client_mode_a =
            blink::mojom::ManifestLaunchHandler_ClientMode::kFocusExisting;
        client_mode_b =
            blink::mojom::ManifestLaunchHandler_ClientMode::kFocusExisting;
        break;
      case ClientModeCombination::kAppANavigateExistingAppBFocusExisting:
        client_mode_a =
            blink::mojom::ManifestLaunchHandler_ClientMode::kNavigateExisting;
        client_mode_b =
            blink::mojom::ManifestLaunchHandler_ClientMode::kFocusExisting;
        break;
    }

    const webapps::AppId app_a = InstallTestWebApp(
        embedded_test_server()->GetURL(kDestinationPageScopeA), client_mode_a);
    const webapps::AppId app_b = InstallTestWebApp(
        embedded_test_server()->GetURL(kDestinationPageScopeB), client_mode_b);

    switch (GetLinkCapturing()) {
      case LinkCapturing::kEnabled:
      case LinkCapturing::kEnabledViaClientMode:
#if BUILDFLAG(IS_CHROMEOS)
        ASSERT_EQ(apps::test::EnableLinkCapturingByUser(profile(), app_a),
                  base::ok());
        ASSERT_EQ(apps::test::EnableLinkCapturingByUser(profile(), app_b),
                  base::ok());
#endif
        break;
      case LinkCapturing::kDisabled:
        ASSERT_EQ(apps::test::DisableLinkCapturingByUser(profile(), app_a),
                  base::ok());
        ASSERT_EQ(apps::test::DisableLinkCapturingByUser(profile(), app_b),
                  base::ok());
        break;
    }

    DLOG(INFO) << "Setting up.";

    MaybeCustomSetup(app_a, app_b);

    // Setup the initial page.
    Browser* browser_a;
    content::WebContents* contents_a;
    {
      if (StartInAppWindow()) {
        GURL url_a = embedded_test_server()->GetURL(kStartPageScopeA);
        contents_a = LaunchStartPageAsApp(app_a, url_a);
      } else {
        EnsureValidNewTabPage();
        GURL url_a = embedded_test_server()->GetURL(kStartPageScopeA);
        contents_a = LaunchPageInTab(url_a);
      }

      // Verify that the the start page is actually ready. This should be
      // guaranteed by waiting for the FinishedNavigation message above, but
      // bugs in the test setup can cause us to not wait for enough navigations
      // to finish, resulting in hard to debug test failures. This assertion
      // intends to make it easier to detect these cases.
      ASSERT_EQ(true, content::EvalJs(contents_a, "isReady"))
          << "Page signaled navigation finished, but is not yet ready. This "
             "could mean the test setup didn't wait for enough navigations to "
             "finish.";

      browser_a = chrome::FindBrowserWithTab(contents_a);
      ASSERT_TRUE(browser_a != nullptr);
      ASSERT_EQ(StartInAppWindow() ? Browser::Type::TYPE_APP
                                   : Browser::Type::TYPE_NORMAL,
                browser_a->type());
    }
    // Ensure that all `WebContents` has finished loading.
    test::CompletePageLoadForAllWebContents();

    DLOG(INFO) << "Performing action.";

    action_histogram_tester_ = std::make_unique<base::HistogramTester>();

    // Perform action (launch destination page).
    WebContentsCreationMonitor monitor;
    {
      content::DOMMessageQueue message_queue;
      // True if a navigation is expected, which will trigger a dom reply.
      bool expect_navigation = true;

      if (GetNavigationElement() == NavigationElement::kElementIntentPicker) {
        ASSERT_NO_FATAL_FAILURE(ClickIntentPickerChip(browser_a));

        // This assumption holds because the Intent Picker w/kNavigateExisting
        // (and kFocusExisting) is tested in a separate test suite.
        expect_navigation = false;
      } else if (ClickMethod() != test::ClickMethod::kRightClickLaunchApp) {
        ASSERT_TRUE(
            IsElementInPage(contents_a->GetPrimaryMainFrame(), GetElementId()));
        test::SimulateClickOnElement(contents_a, GetElementId(), ClickMethod());
      } else {
        ASSERT_TRUE(
            IsElementInPage(contents_a->GetPrimaryMainFrame(), GetElementId()));
        SimulateRightClickOnElementAndLaunchApp(contents_a, GetElementId());
      }

      if (expect_navigation) {
        WaitForNavigationFinishedMessage(message_queue);
      }
    }

    // Ensure that all `WebContents` has finished loading or has been destroyed
    // as needed.
    test::CompletePageLoadForAllWebContents();
    GetNewContentsAndPropagationOfLaunchParams(monitor);

    if (ShouldRebaseline()) {
      RecordActualResults(GetExpectationsFileConfigFromTestConfig(GetParam()));
    } else {
      const base::Value::Dict& test_case = GetTestCaseDataFromParam();
      const base::Value::Dict* expected_state =
          test_case.FindDict("expected_state");
      ASSERT_TRUE(expected_state);
      ASSERT_EQ(*expected_state, CaptureCurrentState());
    }
  }

  // Histogram tester for the action (navigation) that is performed in the test.
  std::unique_ptr<base::HistogramTester> action_histogram_tester_;

 private:
  bool ShouldSkipCurrentTest() {
    // Don't skip any tests if `--run-all-tests` is passed to the test runner.
    if (ShouldRunDisabledTests()) {
      return false;
    }

    testing::TestParamInfo<LinkCaptureTestParam> param(GetParam(), 0);
    const base::Value::Dict& test_case = GetTestCaseDataFromParam();

    // Skip current test-case if the test is disabled in the expectations file.
    // If the "disabled" value is a string (which can be used to specify why the
    // test is disabled), then also consider it disabled.
    if (test_case.FindBool("disabled").value_or(false) ||
        test_case.FindString("disabled")) {
      return true;
    }

    // Skip tests that are disabled because they are flaky.
    if (base::Contains(disabled_flaky_tests, TupleToParamString(param.param)) ||
        base::Contains(disabled_flaky_tests, "*")) {
      return true;
    }

    return false;
  }

  // Returns the path to the test expectation file (or an error).
  base::expected<base::FilePath, std::string> GetPathForLinkCaptureInputJson(
      ExpectationsFileConfig file_config) {
    base::FilePath chrome_src_dir;
    if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT,
                                &chrome_src_dir)) {
      return base::unexpected("Could not find src directory.");
    }
    return GetExpectationsFile(file_config);
  }

  Browser::Type StringToBrowserType(std::string type) {
    if (type == "TYPE_NORMAL") {
      return Browser::Type::TYPE_NORMAL;
    }
    if (type == "TYPE_POPUP") {
      return Browser::Type::TYPE_POPUP;
    }
    if (type == "TYPE_APP") {
      return Browser::Type::TYPE_APP;
    }
    if (type == "TYPE_DEVTOOLS") {
      return Browser::Type::TYPE_DEVTOOLS;
    }
    if (type == "TYPE_APP_POPUP") {
      return Browser::Type::TYPE_APP_POPUP;
    }
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (type == "TYPE_CUSTOM_TAB") {
      return Browser::Type::TYPE_CUSTOM_TAB;
    }
#endif
    if (type == "TYPE_PICTURE_IN_PICTURE") {
      return Browser::Type::TYPE_PICTURE_IN_PICTURE;
    }

    NOTREACHED() << "Unknown browser type: " + type;
  }

  // Parses the json test expectation file. Note that if the expectations file
  // doesn't exist during rebaselining, a dummy json file is used.
  void InitializeTestExpectations(ExpectationsFileConfig file_config) {
    base::ScopedAllowBlockingForTesting allow_blocking;

    std::string json_data;
    // To help with rebaseline conflicts, try a couple times.
    file_read_success_ = false;
    for (int i = 0; i < 3 && !file_read_success_; ++i) {
      file_read_success_ =
          ReadFileToString(GetExpectationsFile(file_config), &json_data);
    }
    // If we're doing a normal test (AKA not doing a rebaseline and not cleaning
    // up expectations), then fail early if the expectation file cannot be read.
    // Cleanup tests will call this with every combination of file
    // configuration, and if no tests exist then the load will fail.
    if (!ShouldRebaseline() &&
        !base::Contains(std::string(::testing::UnitTest::GetInstance()
                                        ->current_test_info()
                                        ->name()),
                        "Cleanup")) {
      ASSERT_TRUE(file_read_success_)
          << "Failed to read test baselines from "
          << GetExpectationsFile(file_config).value();
    }
    if (!file_read_success_) {
      LOG(ERROR) << "Could not read file, loading empty json.";
      json_data = R"({
          "tests": {}
        })";
    }
    test_expectations_ = base::JSONReader::Read(json_data);
    ASSERT_TRUE(test_expectations_) << "Unable to read test expectation file";
    ASSERT_TRUE(test_expectations_.value().is_dict());
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<NotificationDisplayServiceTester> notification_tester_;

  // Current expectations for this test (parsed from the test json file).
  bool file_read_success_ = false;
  std::optional<base::Value> test_expectations_;

  // Prevent multiple redirections from triggering for an intermediate step in a
  // redirection that matches the end site, preventing an infinite loop and a
  // Chrome error page from showing up.
  bool did_redirect_ = false;
};

// IMPORTANT NOTE TO GARDENERS:
//
// Please do not disable tests by adding BUILDFLAGs. The current test class
// runs the same test code for roughly ~700+ parameters, and using a BUILDFLAG
// will disable the whole test suite for an OS, which is an overkill if the
// intention is to disable only a few tests.
//
// Instead, to disable individual test cases, please refer to the documentation
// above the `disabled_flaky_tests` declaration inside this file.
IN_PROC_BROWSER_TEST_P(WebAppLinkCapturingParameterizedBrowserTest,
                       CheckLinkCaptureCombinations) {
  RunTest();
}

// TODO(crbug.com/359600606): Enable on CrOS if needed.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_CleanupExpectations DISABLED_CleanupExpectations
#else
#define MAYBE_CleanupExpectations CleanupExpectations
#endif  // BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(WebAppLinkCapturingParameterizedBrowserTest,
                       MAYBE_CleanupExpectations) {
  PerformTestCleanupIfNeeded(
      {AppUserDisplayMode::kBothBrowser, LinkCapturing::kEnabled});
  PerformTestCleanupIfNeeded(
      {AppUserDisplayMode::kBothStandalone, LinkCapturing::kEnabled});
  PerformTestCleanupIfNeeded({AppUserDisplayMode::kAppAStandaloneAppBBrowser,
                              LinkCapturing::kEnabled});
  PerformTestCleanupIfNeeded(
      {AppUserDisplayMode::kBothBrowser, LinkCapturing::kDisabled});
  PerformTestCleanupIfNeeded(
      {AppUserDisplayMode::kBothStandalone, LinkCapturing::kDisabled});
  PerformTestCleanupIfNeeded({AppUserDisplayMode::kAppAStandaloneAppBBrowser,
                              LinkCapturing::kDisabled});
}

std::string LinkCaptureTestParamToString(
    const testing::TestParamInfo<LinkCaptureTestParam>& param_info) {
  return TupleToParamString(param_info.param);
}

// Pro-tip: To run only one combination from the below list, supply this...
// WebAppLinkCapturingParameterizedBrowserTest.CheckLinkCaptureCombinations/foo
// Where foo can be:
// CaptureOn_AppWnd_ScopeA2A_Direct_ViaLink_LeftClick_WithOpener_TargetSelf
// See ParamToString above for possible values.
INSTANTIATE_TEST_SUITE_P(
    All,
    WebAppLinkCapturingParameterizedBrowserTest,
    testing::Combine(
        testing::Values(ClientModeCombination::kAuto),
        testing::Values(AppUserDisplayMode::kBothStandalone),
        testing::Values(LinkCapturing::kEnabled, LinkCapturing::kDisabled),
        testing::Values(StartingPoint::kAppWindow, StartingPoint::kTab),
        testing::Values(Destination::kScopeA2A,
                        Destination::kScopeA2B,
                        Destination::kScopeA2X),
        testing::Values(RedirectType::kNone),
        testing::Values(NavigationElement::kElementLink,
                        NavigationElement::kElementButton),
        testing::Values(test::ClickMethod::kLeftClick,
                        test::ClickMethod::kMiddleClick,
                        test::ClickMethod::kShiftClick),
        testing::Values(OpenerMode::kOpener, OpenerMode::kNoOpener),
        testing::Values(NavigationTarget::kSelf,
                        NavigationTarget::kFrame,
                        NavigationTarget::kBlank,
                        NavigationTarget::kNoFrame)),
    LinkCaptureTestParamToString);

INSTANTIATE_TEST_SUITE_P(
    DisplayBrowser,
    WebAppLinkCapturingParameterizedBrowserTest,
    testing::Combine(
        testing::Values(ClientModeCombination::kAuto),
        testing::Values(AppUserDisplayMode::kAppAStandaloneAppBBrowser),
        testing::Values(LinkCapturing::kEnabled, LinkCapturing::kDisabled),
        testing::Values(StartingPoint::kAppWindow, StartingPoint::kTab),
        testing::Values(Destination::kScopeA2B),
        testing::Values(RedirectType::kNone),
        testing::Values(NavigationElement::kElementLink,
                        NavigationElement::kElementButton),
        testing::Values(test::ClickMethod::kLeftClick,
                        test::ClickMethod::kMiddleClick,
                        test::ClickMethod::kShiftClick),
        testing::Values(OpenerMode::kOpener, OpenerMode::kNoOpener),
        testing::Values(NavigationTarget::kBlank)),
    LinkCaptureTestParamToString);

INSTANTIATE_TEST_SUITE_P(
    RightClickNavigateNew,
    WebAppLinkCapturingParameterizedBrowserTest,
    testing::Combine(
        testing::Values(ClientModeCombination::kAuto),
        testing::Values(AppUserDisplayMode::kBothStandalone),
        testing::Values(LinkCapturing::kEnabled),
        testing::Values(StartingPoint::kAppWindow, StartingPoint::kTab),
        testing::Values(Destination::kScopeA2A, Destination::kScopeA2B),
        testing::Values(RedirectType::kNone),
        testing::Values(NavigationElement::kElementLink),
        testing::Values(test::ClickMethod::kRightClickLaunchApp),
        testing::Values(OpenerMode::kOpener, OpenerMode::kNoOpener),
        testing::Values(NavigationTarget::kSelf,
                        NavigationTarget::kFrame,
                        NavigationTarget::kBlank,
                        NavigationTarget::kNoFrame)),
    LinkCaptureTestParamToString);

INSTANTIATE_TEST_SUITE_P(
    IntentPicker,
    WebAppLinkCapturingParameterizedBrowserTest,
    testing::Combine(
        // TODO(https://crbug.com/371513459): Test more client modes.
        testing::Values(ClientModeCombination::kAuto),
        testing::Values(AppUserDisplayMode::kBothStandalone),
        // There is really only one combination that makes sense for the rest of
        // the values, since the IntentPicker is not affected by LinkCapturing,
        // it only shows in a Tab (not an App), it always stays within the same
        // scope, and the user only left-clicks it. Additionally, since it is
        // not an HTML element, there's no `opener` or `target` involved.
        testing::Values(LinkCapturing::kEnabled),
        testing::Values(StartingPoint::kTab),
        testing::Values(Destination::kScopeA2A),
        testing::Values(RedirectType::kNone),
        testing::Values(NavigationElement::kElementIntentPicker),
        testing::Values(test::ClickMethod::kLeftClick),
        testing::Values(OpenerMode::kNoOpener),
        testing::Values(NavigationTarget::kNoFrame)),
    LinkCaptureTestParamToString);

INSTANTIATE_TEST_SUITE_P(
    ServiceWorker,
    WebAppLinkCapturingParameterizedBrowserTest,
    testing::Combine(
        testing::Values(ClientModeCombination::kAuto),
        testing::Values(AppUserDisplayMode::kBothStandalone),
        testing::Values(LinkCapturing::kEnabled, LinkCapturing::kDisabled),
        testing::Values(StartingPoint::kAppWindow, StartingPoint::kTab),
        testing::Values(Destination::kScopeA2A, Destination::kScopeA2B),
        testing::Values(RedirectType::kNone),
        testing::Values(NavigationElement::kElementServiceWorkerButton),
        testing::Values(test::ClickMethod::kLeftClick),
        testing::Values(OpenerMode::kNoOpener),
        testing::Values(NavigationTarget::kBlank)),
    LinkCaptureTestParamToString);

INSTANTIATE_TEST_SUITE_P(
    Capturable,
    WebAppLinkCapturingParameterizedBrowserTest,
    testing::Combine(
        testing::Values(ClientModeCombination::kBothFocusExisting,
                        ClientModeCombination::kBothNavigateExisting),
        testing::Values(AppUserDisplayMode::kBothStandalone),
        testing::Values(LinkCapturing::kEnabled, LinkCapturing::kDisabled),
        testing::Values(StartingPoint::kAppWindow, StartingPoint::kTab),
        testing::Values(Destination::kScopeA2A, Destination::kScopeA2B),
        // TODO: Add redirection cases.
        testing::Values(RedirectType::kNone),
        testing::Values(NavigationElement::kElementLink,
                        NavigationElement::kElementButton),
        testing::Values(test::ClickMethod::kLeftClick),
        testing::Values(OpenerMode::kNoOpener),
        testing::Values(NavigationTarget::kBlank)),
    LinkCaptureTestParamToString);

// Tests for browser-tab apps
INSTANTIATE_TEST_SUITE_P(
    DisplayModeBrowser,
    WebAppLinkCapturingParameterizedBrowserTest,
    testing::Combine(
        testing::Values(ClientModeCombination::kAuto,
                        ClientModeCombination::kBothFocusExisting,
                        ClientModeCombination::kBothNavigateExisting),
        testing::Values(AppUserDisplayMode::kBothBrowser),
        testing::Values(LinkCapturing::kEnabled),
        testing::Values(StartingPoint::kTab),
        testing::Values(Destination::kScopeA2A, Destination::kScopeA2B),
        // TODO(crbug.com/375619465): Test redirection.
        testing::Values(RedirectType::kNone),
        testing::Values(NavigationElement::kElementLink),
        testing::Values(test::ClickMethod::kLeftClick),
        testing::Values(OpenerMode::kNoOpener),
        testing::Values(NavigationTarget::kBlank)),
    LinkCaptureTestParamToString);

// Test that the navigate-existing and focus-existing behavior works for browser
// apps if there isn't a tab open. There is a
// NavigationCapturingTestWithAppBLaunched suite version for testing the client
// mode specifically.
INSTANTIATE_TEST_SUITE_P(
    CapturableToBrowserTabApp,
    WebAppLinkCapturingParameterizedBrowserTest,
    testing::Combine(
        testing::Values(ClientModeCombination::kBothFocusExisting,
                        ClientModeCombination::kBothNavigateExisting),
        testing::Values(AppUserDisplayMode::kAppAStandaloneAppBBrowser),
        testing::Values(LinkCapturing::kEnabled, LinkCapturing::kDisabled),
        testing::Values(StartingPoint::kAppWindow, StartingPoint::kTab),
        testing::Values(Destination::kScopeA2B),
        // TODO: Add redirection cases.
        testing::Values(RedirectType::kNone),
        testing::Values(NavigationElement::kElementLink),
        testing::Values(test::ClickMethod::kLeftClick),
        testing::Values(OpenerMode::kNoOpener),
        testing::Values(NavigationTarget::kBlank)),
    LinkCaptureTestParamToString);

// Use-case where redirection goes through intermediary installed apps before
// ending up as a new tab in an existing browser for user modified clicks.
INSTANTIATE_TEST_SUITE_P(
    Redirection_OpenInChrome,
    WebAppLinkCapturingParameterizedBrowserTest,
    testing::Combine(testing::Values(ClientModeCombination::kAuto),
                     testing::Values(AppUserDisplayMode::kBothStandalone),
                     testing::Values(LinkCapturing::kEnabled),
                     testing::Values(StartingPoint::kAppWindow),
                     testing::Values(Destination::kScopeA2X),
                     testing::Values(RedirectType::kServerSideViaA,
                                     RedirectType::kServerSideViaB),
                     testing::Values(NavigationElement::kElementLink),
                     testing::Values(test::ClickMethod::kShiftClick,
                                     test::ClickMethod::kMiddleClick),
                     testing::Values(OpenerMode::kOpener),
                     testing::Values(NavigationTarget::kBlank)),
    LinkCaptureTestParamToString);

INSTANTIATE_TEST_SUITE_P(
    NavigateNew_ServerRedirect_AtoA_StartInApp,
    WebAppLinkCapturingParameterizedBrowserTest,
    testing::Combine(
        testing::Values(ClientModeCombination::kBothFocusExisting,
                        ClientModeCombination::kBothNavigateExisting),
        testing::Values(AppUserDisplayMode::kBothStandalone),
        testing::Values(LinkCapturing::kEnabled),
        testing::Values(StartingPoint::kAppWindow),
        testing::Values(Destination::kScopeA2A),
        testing::Values(RedirectType::kServerSideViaB),
        testing::Values(NavigationElement::kElementLink),
        testing::Values(test::ClickMethod::kLeftClick),
        testing::Values(OpenerMode::kNoOpener),
        testing::Values(NavigationTarget::kBlank)),
    LinkCaptureTestParamToString);

INSTANTIATE_TEST_SUITE_P(
    NavigateNew_ServerRedirect_AtoA_StartInTab,
    WebAppLinkCapturingParameterizedBrowserTest,
    testing::Combine(
        testing::Values(ClientModeCombination::kBothFocusExisting,
                        ClientModeCombination::kBothNavigateExisting),
        testing::Values(AppUserDisplayMode::kAppAStandaloneAppBBrowser),
        testing::Values(LinkCapturing::kEnabled),
        testing::Values(StartingPoint::kTab),
        testing::Values(Destination::kScopeA2A),
        testing::Values(RedirectType::kServerSideViaB),
        testing::Values(NavigationElement::kElementLink),
        testing::Values(test::ClickMethod::kLeftClick),
        testing::Values(OpenerMode::kNoOpener),
        testing::Values(NavigationTarget::kBlank)),
    LinkCaptureTestParamToString);

// Use-case where redirection goes into a browser tab as an intermediate step
// and ends up in an app window, triggered by a shift click.
INSTANTIATE_TEST_SUITE_P(
    Redirection_OpenInApp_NewWindowDisposition,
    WebAppLinkCapturingParameterizedBrowserTest,
    testing::Combine(testing::Values(ClientModeCombination::kAuto),
                     testing::Values(AppUserDisplayMode::kBothStandalone),
                     testing::Values(LinkCapturing::kEnabled),
                     testing::Values(StartingPoint::kAppWindow),
                     testing::Values(Destination::kScopeA2A,
                                     Destination::kScopeA2B),
                     testing::Values(RedirectType::kServerSideViaX),
                     testing::Values(NavigationElement::kElementLink),
                     testing::Values(test::ClickMethod::kShiftClick),
                     testing::Values(OpenerMode::kOpener),
                     testing::Values(NavigationTarget::kBlank)),
    LinkCaptureTestParamToString);

// This is meant to test (most) of the user-modified click scenarios that
// include browser tab apps.
INSTANTIATE_TEST_SUITE_P(
    Redirect_Modified_BrowserApp,
    WebAppLinkCapturingParameterizedBrowserTest,
    testing::Combine(
        testing::Values(ClientModeCombination::kAuto),
        testing::Values(AppUserDisplayMode::kAppAStandaloneAppBBrowser),
        testing::Values(LinkCapturing::kEnabled),
        testing::Values(StartingPoint::kAppWindow, StartingPoint::kTab),
        testing::Values(Destination::kScopeA2A,
                        Destination::kScopeA2B,
                        Destination::kScopeA2X),
        testing::Values(RedirectType::kServerSideViaA,
                        RedirectType::kServerSideViaB,
                        RedirectType::kServerSideViaX),
        testing::Values(NavigationElement::kElementLink),
        testing::Values(test::ClickMethod::kShiftClick,
                        test::ClickMethod::kMiddleClick),
        testing::Values(OpenerMode::kNoOpener),
        testing::Values(NavigationTarget::kBlank)),
    LinkCaptureTestParamToString);

// Test the browser-tab-app -> browser-tab-app user modified redirect.
INSTANTIATE_TEST_SUITE_P(
    Redirect_Modified_BothBrowserApp,
    WebAppLinkCapturingParameterizedBrowserTest,
    testing::Combine(testing::Values(ClientModeCombination::kAuto),
                     testing::Values(AppUserDisplayMode::kBothBrowser),
                     testing::Values(LinkCapturing::kEnabled),
                     testing::Values(StartingPoint::kTab),
                     testing::Values(Destination::kScopeA2A),
                     testing::Values(RedirectType::kServerSideViaB),
                     testing::Values(NavigationElement::kElementLink),
                     testing::Values(test::ClickMethod::kShiftClick),
                     testing::Values(OpenerMode::kNoOpener),
                     testing::Values(NavigationTarget::kBlank)),
    LinkCaptureTestParamToString);

// Test 'navigate-new' interaction with browser apps and redirection.
INSTANTIATE_TEST_SUITE_P(
    Redirect_CaptureNew_BrowserApp,
    WebAppLinkCapturingParameterizedBrowserTest,
    testing::Combine(
        testing::Values(ClientModeCombination::kAuto),
        testing::Values(AppUserDisplayMode::kAppAStandaloneAppBBrowser),
        testing::Values(LinkCapturing::kEnabled),
        testing::Values(StartingPoint::kTab),
        testing::Values(Destination::kScopeA2A,
                        Destination::kScopeA2B,
                        Destination::kScopeA2X),
        testing::Values(RedirectType::kServerSideViaA,
                        RedirectType::kServerSideViaB,
                        RedirectType::kServerSideViaX),
        testing::Values(NavigationElement::kElementLink),
        testing::Values(test::ClickMethod::kLeftClick),
        testing::Values(OpenerMode::kNoOpener),
        testing::Values(NavigationTarget::kBlank)),
    LinkCaptureTestParamToString);

// Use-case where redirection goes into a browser tab as an intermediate step,
// and ends up in an app window, triggered via a middle click.
INSTANTIATE_TEST_SUITE_P(
    Redirection_BackgroundDisposition,
    WebAppLinkCapturingParameterizedBrowserTest,
    testing::Combine(testing::Values(ClientModeCombination::kAuto),
                     testing::Values(AppUserDisplayMode::kBothStandalone),
                     testing::Values(LinkCapturing::kEnabled),
                     testing::Values(StartingPoint::kAppWindow),
                     testing::Values(Destination::kScopeA2A),
                     testing::Values(RedirectType::kServerSideViaB,
                                     RedirectType::kServerSideViaX),
                     testing::Values(NavigationElement::kElementLink),
                     testing::Values(test::ClickMethod::kMiddleClick),
                     testing::Values(OpenerMode::kOpener),
                     testing::Values(NavigationTarget::kBlank)),
    LinkCaptureTestParamToString);

// Use-case where redirection goes into an intermediary target that matches the
// final target app_id as a result of an user modified click.
INSTANTIATE_TEST_SUITE_P(
    Redirection_NavigateCurrent,
    WebAppLinkCapturingParameterizedBrowserTest,
    testing::Combine(testing::Values(ClientModeCombination::kAuto),
                     testing::Values(AppUserDisplayMode::kBothStandalone),
                     testing::Values(LinkCapturing::kEnabled),
                     testing::Values(StartingPoint::kAppWindow),
                     testing::Values(Destination::kScopeA2B),
                     testing::Values(RedirectType::kServerSideViaB),
                     testing::Values(NavigationElement::kElementLink),
                     testing::Values(test::ClickMethod::kMiddleClick,
                                     test::ClickMethod::kShiftClick),
                     testing::Values(OpenerMode::kOpener),
                     testing::Values(NavigationTarget::kBlank)),
    LinkCaptureTestParamToString);

// Use-case where redirection happens via a capturable navigation where a new
// app window was opened intermittently, triggered via a left click.
INSTANTIATE_TEST_SUITE_P(
    Redirection_Capturable_Reparenting,
    WebAppLinkCapturingParameterizedBrowserTest,
    testing::Combine(
        testing::Values(ClientModeCombination::kBothNavigateNew),
        testing::Values(AppUserDisplayMode::kBothStandalone),
        testing::Values(LinkCapturing::kEnabled),
        testing::Values(StartingPoint::kAppWindow, StartingPoint::kTab),
        testing::Values(Destination::kScopeA2B, Destination::kScopeA2X),
        testing::Values(RedirectType::kServerSideViaA,
                        RedirectType::kServerSideViaB),
        testing::Values(NavigationElement::kElementLink,
                        NavigationElement::kElementButton),
        testing::Values(test::ClickMethod::kLeftClick),
        testing::Values(OpenerMode::kNoOpener),
        testing::Values(NavigationTarget::kBlank)),
    LinkCaptureTestParamToString);

// Use-case where redirection happens via a capturable navigation for a navigate
// existing or focus existing launch handler that do not have an app window
// opened already, triggered via a left click.
INSTANTIATE_TEST_SUITE_P(
    Redirection_Capturable_Navigate_And_Focus_Existing_Reparenting,
    WebAppLinkCapturingParameterizedBrowserTest,
    testing::Combine(
        testing::Values(ClientModeCombination::kBothNavigateExisting,
                        ClientModeCombination::kBothFocusExisting),
        testing::Values(AppUserDisplayMode::kBothStandalone),
        testing::Values(LinkCapturing::kEnabled),
        testing::Values(StartingPoint::kTab),
        testing::Values(Destination::kScopeA2B, Destination::kScopeA2X),
        testing::Values(RedirectType::kServerSideViaA),
        testing::Values(NavigationElement::kElementLink,
                        NavigationElement::kElementButton),
        testing::Values(test::ClickMethod::kLeftClick),
        testing::Values(OpenerMode::kNoOpener),
        testing::Values(NavigationTarget::kBlank)),
    LinkCaptureTestParamToString);

// Use-case where redirection happens via an 'Open link in <App>' selection
// from the context menu, triggered via a right click.
INSTANTIATE_TEST_SUITE_P(
    Redirection_RightClickUseCases,
    WebAppLinkCapturingParameterizedBrowserTest,
    testing::Combine(testing::Values(ClientModeCombination::kAuto),
                     testing::Values(mojom::UserDisplayMode::kStandalone),
                     testing::Values(LinkCapturing::kEnabled),
                     testing::Values(StartingPoint::kAppWindow,
                                     StartingPoint::kTab),
                     testing::Values(Destination::kScopeA2B),
                     testing::Values(RedirectType::kServerSideViaA,
                                     RedirectType::kServerSideViaB),
                     testing::Values(NavigationElement::kElementLink),
                     testing::Values(test::ClickMethod::kRightClickLaunchApp),
                     testing::Values(OpenerMode::kNoOpener),
                     testing::Values(NavigationTarget::kBlank)),
    LinkCaptureTestParamToString);

// Tests that verify FORM POST navigations.
INSTANTIATE_TEST_SUITE_P(
    FormPostSubmissions,
    WebAppLinkCapturingParameterizedBrowserTest,
    testing::Combine(testing::Values(ClientModeCombination::kAuto),
                     testing::Values(mojom::UserDisplayMode::kStandalone),
                     testing::Values(LinkCapturing::kEnabled),
                     testing::Values(StartingPoint::kTab),
                     testing::Values(Destination::kScopeA2A,
                                     Destination::kScopeA2B,
                                     Destination::kScopeA2X),
                     testing::Values(RedirectType::kNone),
                     testing::Values(NavigationElement::kElementFormPost),
                     testing::Values(test::ClickMethod::kLeftClick),
                     testing::Values(OpenerMode::kOpener,
                                     OpenerMode::kNoOpener),
                     testing::Values(NavigationTarget::kBlank)),
    LinkCaptureTestParamToString);

// kEnabledViaClientMode should not capture when 'auto' is specified.
INSTANTIATE_TEST_SUITE_P(
    ClientModeEnabledNoCapture,
    WebAppLinkCapturingParameterizedBrowserTest,
    testing::Combine(testing::Values(ClientModeCombination::kAuto),
                     testing::Values(mojom::UserDisplayMode::kStandalone),
                     testing::Values(LinkCapturing::kEnabledViaClientMode),
                     testing::Values(StartingPoint::kTab),
                     testing::Values(Destination::kScopeA2B),
                     testing::Values(RedirectType::kNone),
                     testing::Values(NavigationElement::kElementLink),
                     testing::Values(test::ClickMethod::kLeftClick),
                     testing::Values(OpenerMode::kNoOpener),
                     testing::Values(NavigationTarget::kBlank)),
    LinkCaptureTestParamToString);

// kEnabledViaClientMode should capture when auto isn't specified.
INSTANTIATE_TEST_SUITE_P(
    ClientModeEnabledCaptured,
    WebAppLinkCapturingParameterizedBrowserTest,
    testing::Combine(testing::Values(ClientModeCombination::kBothNavigateNew),
                     testing::Values(mojom::UserDisplayMode::kStandalone),
                     testing::Values(LinkCapturing::kEnabledViaClientMode),
                     testing::Values(StartingPoint::kTab),
                     testing::Values(Destination::kScopeA2B),
                     testing::Values(RedirectType::kNone),
                     testing::Values(NavigationElement::kElementLink),
                     testing::Values(test::ClickMethod::kLeftClick),
                     testing::Values(OpenerMode::kNoOpener),
                     testing::Values(NavigationTarget::kBlank)),
    LinkCaptureTestParamToString);

// This is a derived test fixture that allows us to test Navigation Capturing
// code that relies on an app being launched in the background, so we can
// test e.g. FocusExisting functionality. This additional step is performed
// by overriding MaybeCustomSetup.
//
// For expectations, see navigation_capture_test_launch_app_b.json.
class NavigationCapturingTestWithAppBLaunched
    : public WebAppLinkCapturingParameterizedBrowserTest {
 public:
  std::string GetExpectationsFileBaseName() const override {
    return "navigation_capture_expectations_with_b_launched_in_setup";
  }

  void MaybeCustomSetup(const webapps::AppId& app_a,
                        const webapps::AppId& app_b) override {
    DLOG(INFO) << "Launching App B.";
    content::DOMMessageQueue message_queue;
    ui_test_utils::UrlLoadObserver url_observer(
        WebAppProvider::GetForTest(profile())
            ->registrar_unsafe()
            .GetAppLaunchUrl(app_b));
    base::test::TestFuture<base::WeakPtr<Browser>,
                           base::WeakPtr<content::WebContents>,
                           apps::LaunchContainer>
        launch_future;
    // Note: this respects the user display mode for this app, so this can open
    // in a browser tab or in an app window.
    provider().scheduler().LaunchApp(app_b, /*url=*/std::nullopt,
                                     launch_future.GetCallback());
    ASSERT_TRUE(launch_future.Wait());
    url_observer.Wait();
    // Launching a web app should listen to a single navigation message.
    WaitForNavigationFinishedMessage(message_queue);
  }

  std::string GetTestClassName() const override {
    return "NavigationCapturingTestWithAppBLaunched";
  }
};

IN_PROC_BROWSER_TEST_P(NavigationCapturingTestWithAppBLaunched,
                       CheckLinkCaptureCombinations) {
  RunTest();
}

IN_PROC_BROWSER_TEST_F(NavigationCapturingTestWithAppBLaunched,
                       MAYBE_CleanupExpectations) {
  PerformTestCleanupIfNeeded(
      {AppUserDisplayMode::kBothBrowser, LinkCapturing::kEnabled});
  PerformTestCleanupIfNeeded(
      {AppUserDisplayMode::kBothStandalone, LinkCapturing::kEnabled});
  PerformTestCleanupIfNeeded({AppUserDisplayMode::kAppAStandaloneAppBBrowser,
                              LinkCapturing::kEnabled});
  PerformTestCleanupIfNeeded(
      {AppUserDisplayMode::kBothBrowser, LinkCapturing::kDisabled});
  PerformTestCleanupIfNeeded(
      {AppUserDisplayMode::kBothStandalone, LinkCapturing::kDisabled});
  PerformTestCleanupIfNeeded({AppUserDisplayMode::kAppAStandaloneAppBBrowser,
                              LinkCapturing::kDisabled});
}

// TODO(crbug.com/373495871): Fix flaky tests for kNavigateExisting and enable
// them in navigation_capture_test_launch_app_b.json when fixed.
INSTANTIATE_TEST_SUITE_P(
    RightClickFocusAndNavigateExisting,
    NavigationCapturingTestWithAppBLaunched,
    testing::Combine(
        testing::Values(ClientModeCombination::kBothFocusExisting,
                        ClientModeCombination::kBothNavigateExisting),
        testing::Values(AppUserDisplayMode::kBothStandalone),
        testing::Values(LinkCapturing::kEnabled),
        testing::Values(StartingPoint::kAppWindow, StartingPoint::kTab),
        testing::Values(Destination::kScopeA2B),
        testing::Values(RedirectType::kNone),
        testing::Values(NavigationElement::kElementLink),
        testing::Values(test::ClickMethod::kRightClickLaunchApp),
        testing::Values(OpenerMode::kNoOpener),
        testing::Values(NavigationTarget::kBlank)),
    LinkCaptureTestParamToString);

INSTANTIATE_TEST_SUITE_P(
    CapturableToBrowserTabApp,
    NavigationCapturingTestWithAppBLaunched,
    testing::Combine(
        testing::Values(ClientModeCombination::kBothFocusExisting,
                        ClientModeCombination::kBothNavigateExisting),
        testing::Values(AppUserDisplayMode::kAppAStandaloneAppBBrowser),
        testing::Values(LinkCapturing::kEnabled, LinkCapturing::kDisabled),
        testing::Values(StartingPoint::kAppWindow, StartingPoint::kTab),
        testing::Values(Destination::kScopeA2B),
        // TODO: Add redirection cases.
        testing::Values(RedirectType::kNone),
        testing::Values(NavigationElement::kElementLink),
        testing::Values(test::ClickMethod::kLeftClick),
        testing::Values(OpenerMode::kNoOpener),
        testing::Values(NavigationTarget::kBlank)),
    LinkCaptureTestParamToString);

INSTANTIATE_TEST_SUITE_P(
    Redirect_FocusOrNavigateExisting,
    NavigationCapturingTestWithAppBLaunched,
    testing::Combine(
        testing::Values(
            ClientModeCombination::kAppANavigateExistingAppBFocusExisting,
            ClientModeCombination::kBothNavigateExisting),
        testing::Values(AppUserDisplayMode::kBothStandalone),
        testing::Values(LinkCapturing::kEnabled),
        testing::Values(StartingPoint::kAppWindow, StartingPoint::kTab),
        testing::Values(Destination::kScopeA2B),
        testing::Values(RedirectType::kServerSideViaA,
                        RedirectType::kServerSideViaB,
                        RedirectType::kServerSideViaX),
        testing::Values(NavigationElement::kElementLink),
        testing::Values(test::ClickMethod::kLeftClick),
        testing::Values(OpenerMode::kNoOpener),
        testing::Values(NavigationTarget::kBlank)),
    LinkCaptureTestParamToString);

INSTANTIATE_TEST_SUITE_P(
    Redirect_FocusOrNavigateExisting_Browser,
    NavigationCapturingTestWithAppBLaunched,
    testing::Combine(
        testing::Values(ClientModeCombination::kBothFocusExisting,
                        ClientModeCombination::kBothNavigateExisting),
        testing::Values(AppUserDisplayMode::kAppAStandaloneAppBBrowser,
                        AppUserDisplayMode::kBothBrowser),
        testing::Values(LinkCapturing::kEnabled),
        testing::Values(StartingPoint::kTab),
        testing::Values(Destination::kScopeA2B),  // Navigate A -> B
        testing::Values(RedirectType::kServerSideViaA),
        testing::Values(
            NavigationElement::kElementLink),  // Navigate via element.
        testing::Values(
            test::ClickMethod::kLeftClick),  // Simulate left-mouse click.
        testing::Values(OpenerMode::kNoOpener),
        testing::Values(NavigationTarget::kBlank)),
    LinkCaptureTestParamToString);

// This is a derived test fixture that allows us to test Navigation Capturing
// on focus-existing or navigate-existing launch handlers that open in app
// even if both an app and tab browser are loaded. This additional step is
// performed by overriding MaybeCustomSetup to navigate to a browser tab after
// launch app B.
//
// For expectations, see
// navigation_capturing_with_launched_b_and_browser_tab.json.
class NavigationCapturingTestWithBLaunchedAndBrowserTab
    : public WebAppLinkCapturingParameterizedBrowserTest {
 public:
  std::string GetExpectationsFileBaseName() const override {
    return "navigation_capturing_with_b_lauched_and_browser_tab";
  }

  void MaybeCustomSetup(const webapps::AppId& app_a,
                        const webapps::AppId& app_b) override {
    DLOG(INFO) << "Launching App B.";
    content::DOMMessageQueue message_queue;
    ui_test_utils::UrlLoadObserver url_observer(
        WebAppProvider::GetForTest(profile())
            ->registrar_unsafe()
            .GetAppLaunchUrl(app_b));
    base::test::TestFuture<base::WeakPtr<Browser>,
                           base::WeakPtr<content::WebContents>,
                           apps::LaunchContainer>
        launch_future;
    // Note: this respects the user display mode for this app, so this can open
    // in a browser tab or in an app window.
    provider().scheduler().LaunchApp(app_b, /*url=*/std::nullopt,
                                     launch_future.GetCallback());
    ASSERT_TRUE(launch_future.Wait());
    url_observer.Wait();
    // Launching a web app should listen to a single navigation message.
    WaitForNavigationFinishedMessage(message_queue);

    DLOG(INFO) << "Navigating to browser tab b.";
    EnsureValidNewTabPage();

    GURL url_b_dest = embedded_test_server()->GetURL(kDestinationPageScopeB);
    LaunchPageInTab(url_b_dest);
  }

  std::string GetTestClassName() const override {
    return "NavigationCapturingTestWithBLaunchedAndBrowserTab";
  }
};

IN_PROC_BROWSER_TEST_P(NavigationCapturingTestWithBLaunchedAndBrowserTab,
                       CheckLinkCaptureCombinations) {
  RunTest();
}

IN_PROC_BROWSER_TEST_F(NavigationCapturingTestWithBLaunchedAndBrowserTab,
                       MAYBE_CleanupExpectations) {
  PerformTestCleanupIfNeeded(
      {AppUserDisplayMode::kBothBrowser, LinkCapturing::kEnabled});
  PerformTestCleanupIfNeeded(
      {AppUserDisplayMode::kBothStandalone, LinkCapturing::kEnabled});
  PerformTestCleanupIfNeeded({AppUserDisplayMode::kAppAStandaloneAppBBrowser,
                              LinkCapturing::kEnabled});
  PerformTestCleanupIfNeeded(
      {AppUserDisplayMode::kBothBrowser, LinkCapturing::kDisabled});
  PerformTestCleanupIfNeeded(
      {AppUserDisplayMode::kBothStandalone, LinkCapturing::kDisabled});
  PerformTestCleanupIfNeeded({AppUserDisplayMode::kAppAStandaloneAppBBrowser,
                              LinkCapturing::kDisabled});
}

// TODO(crbug.com/373495871): Fix flaky tests for kNavigateExisting and enable
// them in navigation_capturing_with_b_lauched_and_browser_tab.json when fixed.
INSTANTIATE_TEST_SUITE_P(
    LeftClickToLaunchedAppOverBrowserTab,
    NavigationCapturingTestWithBLaunchedAndBrowserTab,
    testing::Combine(
        testing::Values(ClientModeCombination::kBothFocusExisting,
                        ClientModeCombination::kBothNavigateExisting),
        testing::Values(AppUserDisplayMode::kBothStandalone),
        testing::Values(LinkCapturing::kEnabled),
        testing::Values(StartingPoint::kAppWindow, StartingPoint::kTab),
        testing::Values(Destination::kScopeA2B),
        testing::Values(RedirectType::kNone),
        testing::Values(NavigationElement::kElementLink),
        testing::Values(test::ClickMethod::kLeftClick),
        testing::Values(OpenerMode::kNoOpener),
        testing::Values(NavigationTarget::kBlank)),
    LinkCaptureTestParamToString);

// This is a derived test fixture that allows us to test Navigation Capturing
// on focus-existing or navigate-existing launch handlers that open in a browser
// tab iff there is no app launched and a browser tab is open already.
// This additional step is performed by overriding MaybeCustomSetup to
// navigate to a browser tab.
//
// For expectations, see
// navigation_capturing_with_extra_browser_tab_b.json.
class NavigationCapturingTestWithExtraBrowserTabB
    : public WebAppLinkCapturingParameterizedBrowserTest {
 public:
  std::string GetExpectationsFileBaseName() const override {
    return "navigation_capturing_with_extra_browser_tab_b";
  }

  void MaybeCustomSetup(const webapps::AppId& app_a,
                        const webapps::AppId& app_b) override {
    EnsureValidNewTabPage();
    LaunchPageInTab(embedded_test_server()->GetURL(kDestinationPageScopeB));
  }

  std::string GetTestClassName() const override {
    return "NavigationCapturingTestWithExtraBrowserTabB";
  }
};

IN_PROC_BROWSER_TEST_P(NavigationCapturingTestWithExtraBrowserTabB,
                       CheckLinkCaptureCombinations) {
  RunTest();
}

IN_PROC_BROWSER_TEST_F(NavigationCapturingTestWithExtraBrowserTabB,
                       MAYBE_CleanupExpectations) {
  PerformTestCleanupIfNeeded(
      {AppUserDisplayMode::kBothBrowser, LinkCapturing::kEnabled});
  PerformTestCleanupIfNeeded(
      {AppUserDisplayMode::kBothStandalone, LinkCapturing::kEnabled});
  PerformTestCleanupIfNeeded({AppUserDisplayMode::kAppAStandaloneAppBBrowser,
                              LinkCapturing::kEnabled});
  PerformTestCleanupIfNeeded(
      {AppUserDisplayMode::kBothBrowser, LinkCapturing::kDisabled});
  PerformTestCleanupIfNeeded(
      {AppUserDisplayMode::kBothStandalone, LinkCapturing::kDisabled});
  PerformTestCleanupIfNeeded({AppUserDisplayMode::kAppAStandaloneAppBBrowser,
                              LinkCapturing::kDisabled});
}

INSTANTIATE_TEST_SUITE_P(
    LeftClickToBrowserTabFromFocusOrNavigateExisting,
    NavigationCapturingTestWithExtraBrowserTabB,
    testing::Combine(
        testing::Values(ClientModeCombination::kBothFocusExisting,
                        ClientModeCombination::kBothNavigateExisting),
        testing::Values(AppUserDisplayMode::kBothStandalone),
        testing::Values(LinkCapturing::kEnabled),
        testing::Values(StartingPoint::kAppWindow, StartingPoint::kTab),
        testing::Values(Destination::kScopeA2B),
        testing::Values(RedirectType::kNone),
        testing::Values(NavigationElement::kElementLink),
        testing::Values(test::ClickMethod::kLeftClick),
        testing::Values(OpenerMode::kNoOpener),
        testing::Values(NavigationTarget::kBlank)),
    LinkCaptureTestParamToString);
}  // namespace

}  // namespace web_app
