// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>
#include <string>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/memory/weak_ptr.h"
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
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"
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
};

std::string_view ToParamString(LinkCapturing capturing) {
  switch (capturing) {
    case LinkCapturing::kEnabled:
      return "CaptureOn";
    case LinkCapturing::kDisabled:
      return "CaptureOff";
  }
}

// The starting point for the test:
enum class StartingPoint {
  kAppWindow,
  kTab,
};

std::string_view ToParamString(StartingPoint start) {
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

std::string ToIdString(Destination scope) {
  switch (scope) {
    case Destination::kScopeA2A:
      return kValueScopeA2A;
    case Destination::kScopeA2B:
      return kValueScopeA2B;
    case Destination::kScopeA2X:
      return kValueScopeA2X;
  }
}

std::string_view ToParamString(Destination scope) {
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

std::string_view ToParamString(RedirectType redirect) {
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
  kElementServiceWorkerButton,
  kElementIntentPicker,
};

std::string ToIdString(NavigationElement element) {
  switch (element) {
    case NavigationElement::kElementLink:
      return kValueLink;
    case NavigationElement::kElementButton:
      return kValueButton;
    case NavigationElement::kElementServiceWorkerButton:
      return kValueServiceWorkerButton;
    case NavigationElement::kElementIntentPicker:
      // The IntentPicker is within the Chrome UI, not the web page. Therefore,
      // this should not be used to construct an ID to click on within the page.
      NOTREACHED_NORETURN();
  }
}

std::string_view ToParamString(NavigationElement element) {
  switch (element) {
    case NavigationElement::kElementLink:
      return "ViaLink";
    case NavigationElement::kElementButton:
      return "ViaButton";
    case NavigationElement::kElementServiceWorkerButton:
      return "ViaServiceWorkerButton";
    case NavigationElement::kElementIntentPicker:
      return "ViaIntentPicker";
  }
}

std::string_view ToParamString(test::ClickMethod click) {
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

std::string ToIdString(OpenerMode opener) {
  switch (opener) {
    case OpenerMode::kOpener:
      return kValueOpener;
    case OpenerMode::kNoOpener:
      return kValueNoOpener;
  }
}

std::string ToParamString(
    blink::mojom::ManifestLaunchHandler_ClientMode client_mode) {
  if (client_mode == blink::mojom::ManifestLaunchHandler_ClientMode::kAuto) {
    return "";
  }
  return base::ToString(client_mode);
}

std::string_view ToParamString(OpenerMode opener) {
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

std::string ToIdString(NavigationTarget target) {
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

std::string_view ToParamString(NavigationTarget target) {
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
using LinkCaptureTestParam =
    std::tuple<blink::mojom::ManifestLaunchHandler_ClientMode,
               LinkCapturing,
               StartingPoint,
               Destination,
               RedirectType,
               NavigationElement,
               test::ClickMethod,
               OpenerMode,
               NavigationTarget>;

std::string LinkCaptureTestParamToString(
    testing::TestParamInfo<LinkCaptureTestParam> param_info) {
  // Concatenates the result of calling `ToParamString()` on each member of the
  // tuple with '_' in between fields.
  std::string name = std::apply(
      [](auto&... p) { return base::JoinString({ToParamString(p)...}, "_"); },
      param_info.param);
  base::TrimString(name, "_", &name);
  return name;
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

// Serializes the state of a RenderFrameHost relevant for this test into a
// dictionary that can be stored as JSON. This includes the frame name and
// current URL.
// TODO(crbug.com/359418631): Add opener information to frames if possible.
base::Value::Dict RenderFrameHostToJson(content::RenderFrameHost& rfh) {
  base::Value::Dict dict;
  if (!rfh.GetFrameName().empty()) {
    dict.Set("frame_name", rfh.GetFrameName());
  }
  dict.Set("current_url", rfh.GetLastCommittedURL().path());
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
  if (last_committed_url != GURL("chrome://newtab") &&
      last_committed_url != GURL("chrome://new-tab-page") &&
      last_committed_url != browser.GetNewTabURL()) {
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
    json_entry.Set("url", entry.GetURL().path());
    if (!entry.GetReferrer().url.is_empty()) {
      json_entry.Set("referrer", entry.GetReferrer().url.path());
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
  base::Value::List launchParamsTargetUrls =
      launchParamsResults.ExtractList().TakeList();
  if (!launchParamsTargetUrls.empty()) {
    for (const base::Value& url : launchParamsTargetUrls) {
      dict.EnsureList("launchParams")->Append(GURL(url.GetString()).path());
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
      dict.Set("app_scope", app_scope.path());
    }
  }
  base::Value::List tabs;
  const TabStripModel* tab_model = browser.tab_strip_model();
  for (int i = 0; i < tab_model->count(); ++i) {
    base::Value::Dict tab =
        WebContentsToJson(browser, *tab_model->GetWebContentsAt(i));
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
// 2. Add the `TestParam` under BUILDFLAGs inside the `disabled_flaky_tests` set
// below, to ensure that a single test is only disabled for the OS or builds it
// is flaking on.
// 3. Add the appropriate TODO with a public bug so that the flaky tests can be
// tracked.
//
// Once flakiness has been fixed, please remove the entry from here so that test
// suites can start running the test again.
static const base::flat_set<std::string> disabled_flaky_tests = {
// TODO(crbug.com/372119276): Fix flakiness for `Redirection_OpenInChrome` tests
// on MacOS.
#if BUILDFLAG(IS_MAC)
    "CaptureOn_AppWnd_ScopeA2X_ServerSideViaB_ViaLink_ShiftClick_WithOpener_"
    "TargetBlank",
    "CaptureOn_AppWnd_ScopeA2X_ServerSideViaA_ViaLink_ShiftClick_WithOpener_"
    "TargetBlank",
    "CaptureOn_AppWnd_ScopeA2X_ServerSideViaA_ViaLink_MiddleClick_WithOpener_"
    "TargetBlank"
#elif BUILDFLAG(IS_LINUX)
#elif BUILDFLAG(IS_WIN)
#elif BUILDFLAG(IS_CHROMEOS)
    // TODO(crbug.com/359600606): Enable on CrOS if navigation capturing needs
    // to be supported.
    "*"
#endif
};

// This test verifies the navigation capture logic by testing by launching sites
// inside app containers and tabs and test what happens when links are
// left/middle clicked and window.open is used (whether browser objects are
// reused and what type gets launched).
//
// The test expectations are read from json files that are stored here.
// The main test expectations file:
// chrome/test/data/web_apps/link_capture_test_input.json
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
    std::map<std::string, std::string> parameters;
    parameters["link_capturing_state"] = "reimpl_default_on";
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPwaNavigationCapturing, parameters);
  }

  // Returns the expectations JSON file name without extension
  virtual std::string GetExpectationsFileBaseName() const {
    return "link_capture_test_input";
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
  void WaitForNavigationFinishedMessages(
      content::DOMMessageQueue* message_queue) {
    std::string message;
    EXPECT_TRUE(message_queue->WaitForMessage(&message));
    std::string unquoted_message;
    ASSERT_TRUE(base::RemoveChars(message, "\"", &unquoted_message)) << message;
    EXPECT_TRUE(base::StartsWith(unquoted_message, "FinishedNavigating"))
        << unquoted_message;
    DLOG(INFO) << message;
  }

  base::FilePath GetExpectationsFile() const {
    return base::PathService::CheckedGet(base::DIR_SRC_TEST_DATA_ROOT)
        .AppendASCII(kLinkCaptureTestInputPathPrefix)
        .AppendASCII(GetExpectationsFileBaseName() + ".json");
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
      ASSERT_EQ(blink::mojom::ManifestLaunchHandler_ClientMode::kAuto,
                GetClientMode());
    }

    if (GetNavigationElement() ==
        NavigationElement::kElementServiceWorkerButton) {
      ASSERT_EQ(test::ClickMethod::kLeftClick, ClickMethod());
      ASSERT_EQ(OpenerMode::kNoOpener, GetOpenerMode());
      ASSERT_EQ(NavigationTarget::kBlank, GetNavigationTarget());
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
  //   'TestName': { ... }
  // }}
  // This method returns the dictionary associated with the test name derived
  // from the test parameters. If no entry exists for the test, a new one is
  // created.
  base::Value::Dict& GetTestCaseDataFromParam() {
    testing::TestParamInfo<LinkCaptureTestParam> param(GetParam(), 0);
    base::Value::Dict* result =
        test_expectations().EnsureDict("tests")->EnsureDict(
            LinkCaptureTestParamToString(param));
    // Temporarily check expectations for the test name before redirect mode was
    // a separate parameter as well to make it easier to migrate expectations.
    // TODO(mek): Remove this migration code.
    if (!result->contains("expected_state") &&
        GetRedirectType() == RedirectType::kNone) {
      std::string key = LinkCaptureTestParamToString(param);
      base::ReplaceFirstSubstringAfterOffset(&key, 0, "_Direct", "");
      *result =
          test_expectations().EnsureDict("tests")->EnsureDict(key)->Clone();
      test_expectations().EnsureDict("tests")->Remove(key);
    }
    return *result;
  }

  base::ScopedClosureRunner LockExpectationsFile() {
    CHECK(ShouldRebaseline());
    // Lock the results file to support using `--test-launcher-jobs=X` when
    // doing a rebaseline.
    base::File exclusive_file = base::File(
        lock_file_path_, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_WRITE);

// Fuchsia doesn't support file locking.
#if !BUILDFLAG(IS_FUCHSIA)
    {
      SCOPED_TRACE("Attempting to gain exclusive lock of " +
                   lock_file_path_.MaybeAsASCII());
      base::test::RunUntil([&]() {
        return exclusive_file.Lock(base::File::LockMode::kExclusive) ==
               base::File::FILE_OK;
      });
    }
#endif  // !BUILDFLAG(IS_FUCHSIA)

    // Re-read expectations to catch changes from other parallel runs of
    // rebaselining.
    InitializeTestExpectations();

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
      base::Value::Dict json_browser = BrowserToJson(*b);
      browsers.Append(std::move(json_browser));
    }

    // Checks whether the web app launch metrics have been measured for the
    // current navigation.
    std::vector<base::Bucket> buckets =
        action_histogram_tester_->GetAllSamples("WebApp.LaunchSource");
    base::Value::List bucket_list;
    for (const base::Bucket& bucket : buckets) {
      EXPECT_EQ(1, bucket.count);
      bucket_list.Append(
          base::ToString(static_cast<apps::LaunchSource>(bucket.min)));
    }

    return base::Value::Dict()
        .Set("browsers", std::move(browsers))
        .Set("launch_metric_buckets", std::move(bucket_list));
  }

  // This function is used during rebaselining to record (to a file) the results
  // from an actual run of a single test case, used by developers to update the
  // expectations. Constructs a json dictionary and saves it to the test results
  // json file. Returns true if writing was successful.
  void RecordActualResults() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    // Lock the results file to support using `--test-launcher-jobs=X` when
    // doing a rebaseline.
    base::ScopedClosureRunner lock = LockExpectationsFile();

    base::Value::Dict& test_case = GetTestCaseDataFromParam();
    // If this is a new test case, start it out as disabled until we've manually
    // verified the expectations are correct.
    if (!test_case.contains("expected_state")) {
      test_case.Set("disabled", true);
    }
    test_case.Set("expected_state", CaptureCurrentState());
    SaveExpectations();
  }

  void SaveExpectations() {
    CHECK(ShouldRebaseline());
    // Write formatted JSON back to disk.
    std::optional<std::string> json_string = base::WriteJsonWithOptions(
        *test_expectations_, base::JsonOptions::OPTIONS_PRETTY_PRINT);
    ASSERT_TRUE(json_string.has_value());
    ASSERT_TRUE(base::WriteFile(GetExpectationsFile(), *json_string));
  }

  LinkCapturing GetLinkCapturing() const {
    return std::get<LinkCapturing>(GetParam());
  }

  blink::mojom::ManifestLaunchHandler_ClientMode GetClientMode() const {
    return std::get<blink::mojom::ManifestLaunchHandler_ClientMode>(GetParam());
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

  GURL GetDestinationUrl() {
    switch (GetDestination()) {
      case Destination::kScopeA2A:
        return embedded_test_server()->GetURL(kDestinationPageScopeA);
      case Destination::kScopeA2B:
        return embedded_test_server()->GetURL(kDestinationPageScopeB);
      case Destination::kScopeA2X:
        return embedded_test_server()->GetURL(kDestinationPageScopeX);
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
  std::string GetElementId() {
    return base::JoinString(
        {"id", ToIdString(GetNavigationElement()),
         ToIdString(GetRedirectType(), GetDestination()),
         ToIdString(GetNavigationTarget()), ToIdString(GetOpenerMode())},
        "-");
  }

  webapps::AppId InstallTestWebApp(const GURL& start_url) {
    auto web_app_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
    web_app_info->launch_handler =
        blink::Manifest::LaunchHandler(GetClientMode());
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->display_mode = blink::mojom::DisplayMode::kStandalone;
    const webapps::AppId app_id =
        test::InstallWebApp(profile(), std::move(web_app_info));
    apps::AppReadinessWaiter(profile(), app_id).Await();
    return app_id;
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

    // Parses the corresponding json file for test expectations given the
    // respective test suite.
    InitializeTestExpectations();

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
  void PerformTestCleanupIfNeeded() {
    std::set<std::string> test_cases;
    const testing::UnitTest* unit_test = testing::UnitTest::GetInstance();
    for (int i = 0; i < unit_test->total_test_suite_count(); ++i) {
      const testing::TestSuite* test_suite = unit_test->GetTestSuite(i);
      // We only care about link capturing parameterized tests.
      if (std::string_view(test_suite->name()).find(GetTestClassName()) ==
          std::string::npos) {
        continue;
      }
      for (int j = 0; j < test_suite->total_test_count(); ++j) {
        const char* name = test_suite->GetTestInfo(j)->name();
        auto parts = base::SplitStringOnce(name, '/');
        if (!parts.has_value()) {
          // Not a parameterized test.
          continue;
        }
        test_cases.insert(std::string(parts->second));
      }
    }

    base::ScopedAllowBlockingForTesting allow_blocking;
    base::ScopedClosureRunner lock;
    if (ShouldRebaseline()) {
      lock = LockExpectationsFile();
    }

    base::Value::Dict& expectations = *test_expectations().EnsureDict("tests");
    std::vector<std::string> tests_to_remove;
    for (const auto [name, value] : expectations) {
      if (!test_cases.contains(name)) {
        tests_to_remove.push_back(name);
      }
    }
    if (ShouldRebaseline()) {
      for (const auto& name : tests_to_remove) {
        LOG(INFO) << "Removing " << name;
        expectations.Remove(name);
      }
      SaveExpectations();
    } else {
      EXPECT_THAT(tests_to_remove, testing::ElementsAre())
          << "Run this test with --rebaseline-link-capturing-test to clean "
             "this "
             "up.";
    }
  }

  base::Value::Dict& test_expectations() {
    CHECK(test_expectations_.has_value() && test_expectations_->is_dict());
    return test_expectations_->GetDict();
  }

  void RunTest() {
    if (ShouldSkipCurrentTest()) {
      GTEST_SKIP()
          << "Skipped as test is marked as disabled in the expectations file. "
             "Add the switch '--run-all-tests' to run disabled tests too.";
    }

    AssertValidTestConfiguration();

    DLOG(INFO) << "Installing apps.";

    // Install apps for scope A and B (note: scope X is deliberately excluded).
    const webapps::AppId app_a = InstallTestWebApp(
        embedded_test_server()->GetURL(kDestinationPageScopeA));
    const webapps::AppId app_b = InstallTestWebApp(
        embedded_test_server()->GetURL(kDestinationPageScopeB));

    if (GetLinkCapturing() == LinkCapturing::kDisabled) {
      ASSERT_EQ(apps::test::DisableLinkCapturingByUser(profile(), app_a),
                base::ok());
      ASSERT_EQ(apps::test::DisableLinkCapturingByUser(profile(), app_b),
                base::ok());
    }

    DLOG(INFO) << "Setting up.";

    MaybeCustomSetup(app_a, app_b);

    // Setup the initial page.
    Browser* browser_a;
    content::WebContents* contents_a;
    {
      content::DOMMessageQueue message_queue;

      if (StartInAppWindow()) {
        base::test::TestFuture<base::WeakPtr<Browser>,
                               base::WeakPtr<content::WebContents>,
                               apps::LaunchContainer>
            launch_future;
        provider().scheduler().LaunchApp(
            app_a, embedded_test_server()->GetURL(kStartPageScopeA),
            launch_future.GetCallback());
        ASSERT_TRUE(launch_future.Wait());
        contents_a =
            launch_future.Get<base::WeakPtr<content::WebContents>>().get();
        content::WaitForLoadStop(contents_a);
      } else {
        ASSERT_TRUE(ui_test_utils::NavigateToURL(
            browser(), embedded_test_server()->GetURL(kStartPageScopeA)));
        contents_a = browser()->tab_strip_model()->GetActiveWebContents();
      }

      std::string message;
      EXPECT_TRUE(message_queue.WaitForMessage(&message));
      EXPECT_TRUE(base::Contains(message, "FinishedNavigating")) << message;
      DLOG(INFO) << message;

      browser_a = chrome::FindBrowserWithTab(contents_a);
      ASSERT_TRUE(browser_a != nullptr);
      ASSERT_EQ(StartInAppWindow() ? Browser::Type::TYPE_APP
                                   : Browser::Type::TYPE_NORMAL,
                browser_a->type());
    }

    DLOG(INFO) << "Performing action.";

    action_histogram_tester_ = std::make_unique<base::HistogramTester>();

    {
      content::DOMMessageQueue message_queue;
      // Perform action (launch destination page).
      WebContentsCreationMonitor monitor;
      // True if a navigation is expected, which will trigger a dom reply.
      bool expect_navigation = true;

      if (GetNavigationElement() == NavigationElement::kElementIntentPicker) {
        ui_test_utils::BrowserChangeObserver app_browser_observer(
            nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
        // Clicking the Intent Picker will trigger a re-parenting (not a new
        // navigation, so the DomMessage has already been sent).
        ASSERT_TRUE(web_app::ClickIntentPickerChip(browser_a));
        app_browser_observer.Wait();

        // After re-parenting, the old browser gets a new tab contents and we
        // need to wait for that to finish loading before capturing the end
        // state.
        WaitForLoadStop(browser_a->tab_strip_model()->GetActiveWebContents());

        // TODO(https://crbug.com/371513459): Not sure if this assumption holds
        // if we add kNavigateExisting to the test params (for the Intent
        // Picker).
        expect_navigation = false;
      } else if (ClickMethod() != test::ClickMethod::kRightClickLaunchApp) {
        test::SimulateClickOnElement(contents_a, GetElementId(), ClickMethod());
      } else {
        SimulateRightClickOnElementAndLaunchApp(contents_a, GetElementId());
      }

      if (expect_navigation) {
        std::string message;
        EXPECT_TRUE(message_queue.WaitForMessage(&message));
        DLOG(INFO) << message;
        std::string unquoted_message;
        ASSERT_TRUE(base::RemoveChars(message, "\"", &unquoted_message))
            << message;
        EXPECT_TRUE(base::StartsWith(unquoted_message, "FinishedNavigating"))
            << unquoted_message;
      }

      content::WebContents* handled_contents =
          monitor.GetLastSeenWebContentsAndStopMonitoring();
      ASSERT_NE(nullptr, handled_contents);
      ASSERT_TRUE(handled_contents->GetURL().is_valid());

      provider().command_manager().AwaitAllCommandsCompleteForTesting();
      // Attempt to ensure that all launchParams have propagated.
      content::RunAllTasksUntilIdle();
    }

    if (ShouldRebaseline()) {
      RecordActualResults();
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
    testing::TestParamInfo<LinkCaptureTestParam> param(GetParam(), 0);
    const base::Value::Dict& test_case = GetTestCaseDataFromParam();

    // Skip current test-case if the test is disabled and `--run-all-tests` is
    // not passed to the test runner.
    if (!ShouldRunDisabledTests() &&
        test_case.FindBool("disabled").value_or(false)) {
      return true;
    }

    // Skip tests that are disabled because they are flaky.
    if (base::Contains(disabled_flaky_tests,
                       LinkCaptureTestParamToString(param)) ||
        base::Contains(disabled_flaky_tests, "*")) {
      return true;
    }

    return false;
  }

  // Returns the path to the test expectation file (or an error).
  base::expected<base::FilePath, std::string> GetPathForLinkCaptureInputJson() {
    base::FilePath chrome_src_dir;
    if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT,
                                &chrome_src_dir)) {
      return base::unexpected("Could not find src directory.");
    }
    return GetExpectationsFile();
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
  void InitializeTestExpectations() {
    std::string json_data;
    bool success = ReadFileToString(GetExpectationsFile(), &json_data);
    if (!ShouldRebaseline()) {
      ASSERT_TRUE(success) << "Failed to read test baselines";
    }
    if (!success) {
      json_data = R"(
          {"tests": {}}
        )";
    }
    test_expectations_ = base::JSONReader::Read(json_data);
    ASSERT_TRUE(test_expectations_) << "Unable to read test expectation file";
    ASSERT_TRUE(test_expectations_.value().is_dict());
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<NotificationDisplayServiceTester> notification_tester_;

  base::FilePath lock_file_path_ =
      base::PathService::CheckedGet(base::DIR_OUT_TEST_DATA_ROOT)
          .AppendASCII(GetExpectationsFileBaseName() + "_lock_file.lock");

  // Current expectations for this test (parsed from the test json file).
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
  PerformTestCleanupIfNeeded();
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
        testing::Values(blink::mojom::ManifestLaunchHandler_ClientMode::kAuto),
        testing::Values(LinkCapturing::kEnabled,  // LinkCapturing turned on.
                        LinkCapturing::kDisabled  // LinkCapturing turned off.
                        ),
        testing::Values(
            StartingPoint::kAppWindow,  // Starting point is app window.
            StartingPoint::kTab         // Starting point is a tab.
            ),
        testing::Values(Destination::kScopeA2A,  // Navigate in-scope A.
                        Destination::kScopeA2B,  // Navigate A -> B.
                        Destination::kScopeA2X   // A -> X (X is not installed).
                        ),
        testing::Values(RedirectType::kNone),
        testing::Values(
            NavigationElement::kElementLink,   // Navigate via element.
            NavigationElement::kElementButton  // Navigate via button.
            ),
        testing::Values(
            test::ClickMethod::kLeftClick,    // Simulate left-mouse click.
            test::ClickMethod::kMiddleClick,  // Simulate middle-mouse click.
            test::ClickMethod::kShiftClick    // Simulate shift click.
            ),
        testing::Values(OpenerMode::kOpener,   // Supply 'opener' property.
                        OpenerMode::kNoOpener  // Supply 'noopener' property.
                        ),
        testing::Values(
            NavigationTarget::kSelf,    // Use target _self.
            NavigationTarget::kFrame,   // Use named frame as target.
            NavigationTarget::kBlank,   // User Target is _blank.
            NavigationTarget::kNoFrame  // Target is non-existing frame.
            )),
    LinkCaptureTestParamToString);

INSTANTIATE_TEST_SUITE_P(
    RightClickNavigateNew,
    WebAppLinkCapturingParameterizedBrowserTest,
    testing::Combine(
        // ClientMode::kAuto defaults to NavigateNew on all platforms.
        testing::Values(blink::mojom::ManifestLaunchHandler_ClientMode::kAuto),
        testing::Values(LinkCapturing::kEnabled),  // LinkCapturing turned on.
        testing::Values(
            StartingPoint::kAppWindow,  // Starting point is app window.
            StartingPoint::kTab         // Starting point is a tab.
            ),
        testing::Values(Destination::kScopeA2A,  // Navigate in-scope A.
                        Destination::kScopeA2B   // Navigate A -> B
                        ),
        testing::Values(RedirectType::kNone),
        testing::Values(
            NavigationElement::kElementLink),  // Navigate via element.
        testing::Values(
            test::ClickMethod::kRightClickLaunchApp),  // Simulate right-mouse
                                                       // click.
        testing::Values(OpenerMode::kOpener,   // Supply 'opener' property.
                        OpenerMode::kNoOpener  // Supply 'noopener' property.
                        ),
        testing::Values(
            NavigationTarget::kSelf,    // Use target _self.
            NavigationTarget::kFrame,   // Use named frame as target.
            NavigationTarget::kBlank,   // User Target is _blank.
            NavigationTarget::kNoFrame  // Target is non-existing frame.
            )),
    LinkCaptureTestParamToString);

INSTANTIATE_TEST_SUITE_P(
    IntentPicker,
    WebAppLinkCapturingParameterizedBrowserTest,
    testing::Combine(
        // TODO(https://crbug.com/371513459): Test more client modes.
        testing::Values(blink::mojom::ManifestLaunchHandler_ClientMode::kAuto),
        // There is really only one combination that makes sense for the rest of
        // the values, since the IntentPicker is not affected by LinkCapturing,
        // it only shows in a Tab (not an App), it always stays within the same
        // scope, and the user only left-clicks it. Additionally, since it is
        // not an HTML element, there's no `opener` or `target` involved.
        testing::Values(LinkCapturing::kEnabled),
        testing::Values(StartingPoint::kTab),
        testing::Values(Destination::kScopeA2A),  // Navigate in-scope A.
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
        testing::Values(blink::mojom::ManifestLaunchHandler_ClientMode::kAuto),
        testing::Values(LinkCapturing::kEnabled,  // LinkCapturing turned on.
                        LinkCapturing::kDisabled  // LinkCapturing turned off.
                        ),
        testing::Values(
            StartingPoint::kAppWindow,  // Starting point is app window.
            StartingPoint::kTab         // Starting point is a tab.
            ),
        testing::Values(Destination::kScopeA2A,   // Navigate in-scope A.
                        Destination::kScopeA2B),  // Navigate A -> B.
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
        testing::Values(
            blink::mojom::ManifestLaunchHandler_ClientMode::kFocusExisting,
            blink::mojom::ManifestLaunchHandler_ClientMode::kNavigateExisting),
        testing::Values(LinkCapturing::kEnabled,  // LinkCapturing turned on.
                        LinkCapturing::kDisabled  // LinkCapturing turned off.
                        ),
        testing::Values(StartingPoint::kAppWindow, StartingPoint::kTab),
        testing::Values(Destination::kScopeA2A,  // Navigate A -> A.
                        Destination::kScopeA2B   // Navigate A -> B.
                        ),
        // TODO: Add redirection cases.
        testing::Values(RedirectType::kNone),
        testing::Values(NavigationElement::kElementLink,
                        NavigationElement::kElementButton),
        testing::Values(test::ClickMethod::kLeftClick),
        testing::Values(OpenerMode::kNoOpener),
        testing::Values(NavigationTarget::kBlank)),
    LinkCaptureTestParamToString);

// Use-case where redirection goes through intermediary installed apps before
// ending up as a new tab in an existing browser for user modified clicks.
INSTANTIATE_TEST_SUITE_P(
    Redirection_OpenInChrome,
    WebAppLinkCapturingParameterizedBrowserTest,
    testing::Combine(
        testing::Values(blink::mojom::ManifestLaunchHandler_ClientMode::kAuto),
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

// Use-case where redirection goes into a browser tab as an intermediate step
// and ends up in an app window, triggered by a shift click.
INSTANTIATE_TEST_SUITE_P(
    Redirection_OpenInApp_NewWindowDisposition,
    WebAppLinkCapturingParameterizedBrowserTest,
    testing::Combine(
        testing::Values(blink::mojom::ManifestLaunchHandler_ClientMode::kAuto),
        testing::Values(LinkCapturing::kEnabled),
        testing::Values(StartingPoint::kAppWindow),
        testing::Values(Destination::kScopeA2A, Destination::kScopeA2B),
        testing::Values(RedirectType::kServerSideViaX),
        testing::Values(NavigationElement::kElementLink),
        testing::Values(test::ClickMethod::kShiftClick),
        testing::Values(OpenerMode::kOpener),
        testing::Values(NavigationTarget::kBlank)),
    LinkCaptureTestParamToString);

// Use-case where redirection goes into a browser tab as an intermediate step,
// and ends up in an app window, triggered via a middle click.
INSTANTIATE_TEST_SUITE_P(
    Redirection_BackgroundDisposition,
    WebAppLinkCapturingParameterizedBrowserTest,
    testing::Combine(
        testing::Values(blink::mojom::ManifestLaunchHandler_ClientMode::kAuto),
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
    testing::Combine(
        testing::Values(blink::mojom::ManifestLaunchHandler_ClientMode::kAuto),
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

// This is a derived test fixture that allows us to test Navigation Capturing
// code that relies on an app being launched in the background, so we can
// test e.g. FocusExisting functionality. This additional step is performed
// by overriding MaybeCustomSetup.
//
// For expectations, see navigation_capture_test_launch_app_b.json.
class NavigationCapturingTestWithAppBLaunched
    : public WebAppLinkCapturingParameterizedBrowserTest {
 public:
  // Returns the expectations JSON file name without extension
  std::string GetExpectationsFileBaseName() const override {
    return "navigation_capture_test_launch_app_b";
  }

  void MaybeCustomSetup(const webapps::AppId& app_a,
                        const webapps::AppId& app_b) override {
    DLOG(INFO) << "Launching App B.";
    content::DOMMessageQueue message_queue;
    web_app::LaunchWebAppBrowserAndWait(profile(), app_b);
    // Launching a web app should listen to a single navigation message.
    WaitForNavigationFinishedMessages(&message_queue);
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
  PerformTestCleanupIfNeeded();
}

INSTANTIATE_TEST_SUITE_P(
    RightClickFocusAndNavigateExisting,
    NavigationCapturingTestWithAppBLaunched,
    testing::Combine(
        testing::Values(
            blink::mojom::ManifestLaunchHandler_ClientMode::kFocusExisting),
        testing::Values(LinkCapturing::kEnabled),  // LinkCapturing turned on.
        testing::Values(
            StartingPoint::kAppWindow,  // Starting point is app window.
            StartingPoint::kTab         // Starting point is a tab.
            ),
        testing::Values(Destination::kScopeA2B),  // Navigate A -> B
        testing::Values(RedirectType::kNone),
        testing::Values(
            NavigationElement::kElementLink),  // Navigate via element.
        testing::Values(
            test::ClickMethod::kRightClickLaunchApp),  // Simulate right-mouse
                                                       // click.
        testing::Values(OpenerMode::kOpener,   // Supply 'opener' property.
                        OpenerMode::kNoOpener  // Supply 'noopener' property.
                        ),
        testing::Values(
            NavigationTarget::kSelf,    // Use target _self.
            NavigationTarget::kFrame,   // Use named frame as target.
            NavigationTarget::kBlank,   // User Target is _blank.
            NavigationTarget::kNoFrame  // Target is non-existing frame.
            )),
    LinkCaptureTestParamToString);

}  // namespace

}  // namespace web_app
