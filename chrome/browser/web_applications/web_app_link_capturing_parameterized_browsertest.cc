// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/current_thread.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_restrictions.h"
#include "base/types/expected.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_permission_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/manifest_launch_handler.mojom-shared.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "url/gurl.h"

namespace {

constexpr char kStartPageScopeA[] =
    "/banners/link_capturing/scope_a/start.html";
constexpr char kDestinationPageScopeA[] =
    "/banners/link_capturing/scope_a/destination.html";
constexpr char kDestinationPageScopeB[] =
    "/banners/link_capturing/scope_b/destination.html";
constexpr char kLinkCaptureTestInputPath[] =
    "chrome/test/data/web_apps/link_capture_test_input.json";

constexpr char kValueScopeA2A[] = "A_TO_A";
constexpr char kValueScopeA2B[] = "A_TO_B";
constexpr char kValueLink[] = "LINK";
constexpr char kValueButton[] = "BTN";
constexpr char kValueServiceWorkerButton[] = "BTN_SW";
constexpr char kValueOpener[] = "OPENER";
constexpr char kValueNoOpener[] = "NO_OPENER";
constexpr char kValueTargetSelf[] = "SELF";
constexpr char kValueTargetFrame[] = "FRAME";
constexpr char kValueTargetBlank[] = "BLANK";
constexpr char kValueTargetNoFrame[] = "NO_FRAME";

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

// Whether to navigate within the same scope or outside it:
enum class Destination {
  kScopeA2A,
  kScopeA2B,
};

std::string ToIdString(Destination scope) {
  switch (scope) {
    case Destination::kScopeA2A:
      return kValueScopeA2A;
    case Destination::kScopeA2B:
      return kValueScopeA2B;
  }
}

std::string_view ToParamString(Destination scope) {
  switch (scope) {
    case Destination::kScopeA2A:
      return "ScopeA2A";
    case Destination::kScopeA2B:
      return "ScopeA2B";
  }
}

enum class RedirectType {
  kNone,
  kServerSideViaA,
  kServerSideViaB,
};

std::string ToIdString(RedirectType redirect, Destination final_destination) {
  switch (redirect) {
    case RedirectType::kNone:
      return ToIdString(final_destination);
    case RedirectType::kServerSideViaA:
      return kValueScopeA2A;
    case RedirectType::kServerSideViaB:
      return kValueScopeA2B;
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
  }
}

// The element to use for navigation:
enum class NavigationElement {
  kElementLink,
  kElementButton,
  kElementServiceWorkerButton,
};

std::string ToIdString(NavigationElement element) {
  switch (element) {
    case NavigationElement::kElementLink:
      return kValueLink;
    case NavigationElement::kElementButton:
      return kValueButton;
    case NavigationElement::kElementServiceWorkerButton:
      return kValueServiceWorkerButton;
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
  }
}

// The method of interacting with the element:
enum class ClickMethod { kLeftClick, kMiddleClick, kShiftClick };

std::string_view ToParamString(ClickMethod click) {
  switch (click) {
    case ClickMethod::kLeftClick:
      return "LeftClick";
    case ClickMethod::kMiddleClick:
      return "MiddleClick";
    case ClickMethod::kShiftClick:
      return "ShiftClick";
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
               StartingPoint,
               Destination,
               RedirectType,
               NavigationElement,
               ClickMethod,
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
base::Value::Dict WebContentsToJson(content::WebContents& web_contents) {
  base::Value::Dict dict =
      RenderFrameHostToJson(*web_contents.GetPrimaryMainFrame());
  if (web_contents.HasOpener()) {
    dict.Set("has_opener", true);
  }

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
    web_app::WebAppProvider* provider =
        web_app::WebAppProvider::GetForTest(browser.profile());
    const GURL& app_scope = provider->registrar_unsafe().GetAppScope(app_id);
    if (app_scope.is_valid()) {
      dict.Set("app_scope", app_scope.path());
    }
  }
  base::Value::List tabs;
  const TabStripModel* tab_model = browser.tab_strip_model();
  for (int i = 0; i < tab_model->count(); ++i) {
    base::Value::Dict tab = WebContentsToJson(*tab_model->GetWebContentsAt(i));
    if (i == tab_model->active_index()) {
      tab.Set("active", true);
    }
    tabs.Append(std::move(tab));
  }
  dict.Set("tabs", std::move(tabs));
  return dict;
}

// Serializes the entire state of chrome that we're interested in in this test
// to a dictionary. This state consists of the state of all Browser windows, in
// creation order of the Browser.
base::Value::Dict CaptureCurrentState() {
  base::Value::List browsers;
  for (Browser* b : *BrowserList::GetInstance()) {
    base::Value::Dict json_browser = BrowserToJson(*b);
    browsers.Append(std::move(json_browser));
  }
  return base::Value::Dict().Set("browsers", std::move(browsers));
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

}  // namespace

// This test verifies the link capture logic by testing by launching sites
// inside app containers and tabs and test what happens when links are
// left/middle clicked and window.open is used (whether browser objects are
// reused and what type gets launched).
//
// The test expectations are read from a json file that is stored here:
// chrome/test/data/web_apps/link_capture_test_input.json
//
// The expectations file maps test names (as serialized from the test
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
    : public web_app::WebAppBrowserTestBase,
      public testing::WithParamInterface<LinkCaptureTestParam> {
 public:
  WebAppLinkCapturingParameterizedBrowserTest() {
    std::map<std::string, std::string> parameters;
    parameters["link_capturing_state"] = "reimpl_default_on";
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kDesktopPWAsLinkCapturing, parameters);
    InitializeTestExpectations();
  }

  std::unique_ptr<net::test_server::HttpResponse> SimulateRedirectHandler(
      const net::test_server::HttpRequest& request) {
    if (GetRedirectType() == RedirectType::kNone) {
      return nullptr;  // This test is not using redirects.
    }
    if (request.GetURL().spec().find("/destination.html") ==
        std::string::npos) {
      return nullptr;  // Only redirect for destination pages.
    }

    GURL redirect_from = GetRedirectIntermediateUrl();
    GURL redirect_to = GetDestinationUrl();

    // We don't redirect requests for start.html, manifest files, etc. Only the
    // destination page the test wants to run.
    if (request.GetURL() != redirect_from) {
      return nullptr;
    }

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_TEMPORARY_REDIRECT);
    response->set_content_type("text/html");
    response->AddCustomHeader("Location", redirect_to.spec());
    response->AddCustomHeader("Access-Control-Allow-Origin", "*");
    response->set_content(base::StringPrintf(
        "<!doctype html><p>Redirecting to %s", redirect_to.spec().c_str()));
    return response;
  }

 protected:
  // This function simulates a click on the middle of an element matching
  // `element_id` based on the type of click passed to it.
  void SimulateClickOnElement(content::WebContents* contents,
                              std::string element_id,
                              ClickMethod click) {
    gfx::Point element_center = gfx::ToFlooredPoint(
        content::GetCenterCoordinatesOfElementWithId(contents, element_id));
    int modifiers = 0;
    blink::WebMouseEvent::Button button = blink::WebMouseEvent::Button::kLeft;
    switch (click) {
      case ClickMethod::kLeftClick:
        modifiers = blink::WebInputEvent::Modifiers::kNoModifiers;
        break;
      case ClickMethod::kMiddleClick:
#if BUILDFLAG(IS_MAC)
        modifiers = blink::WebInputEvent::Modifiers::kMetaKey;
#else
        modifiers = blink::WebInputEvent::Modifiers::kControlKey;
#endif  // BUILDFLAG(IS_MAC)
        break;
      case ClickMethod::kShiftClick:
        modifiers = blink::WebInputEvent::Modifiers::kShiftKey;
        break;
    }
    content::SimulateMouseClickAt(contents, modifiers, button, element_center);
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
    ASSERT_TRUE(base::WriteFile(json_file_path_, *json_string));
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
    }
  }

  NavigationElement GetNavigationElement() const {
    return std::get<NavigationElement>(GetParam());
  }

  ClickMethod GetClickMethod() const {
    return std::get<ClickMethod>(GetParam());
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
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->user_display_mode =
        web_app::mojom::UserDisplayMode::kStandalone;
    web_app_info->launch_handler =
        blink::Manifest::LaunchHandler(GetClientMode());
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->display_mode = blink::mojom::DisplayMode::kStandalone;
    const webapps::AppId app_id =
        web_app::test::InstallWebApp(profile(), std::move(web_app_info));
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

  base::Value::Dict& test_expectations() {
    CHECK(test_expectations_.has_value() && test_expectations_->is_dict());
    return test_expectations_->GetDict();
  }

 private:
  // Returns the path to the test expectation file (or an error).
  base::expected<base::FilePath, std::string> GetPathForLinkCaptureInputJson() {
    base::FilePath chrome_src_dir;
    if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT,
                                &chrome_src_dir)) {
      return base::unexpected("Could not find src directory.");
    }
    return chrome_src_dir.AppendASCII(kLinkCaptureTestInputPath);
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
    bool success = ReadFileToString(json_file_path_, &json_data);
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

  // The path to the json file containing the test expectations.
  base::FilePath json_file_path_ =
      base::PathService::CheckedGet(base::DIR_SRC_TEST_DATA_ROOT)
          .AppendASCII(kLinkCaptureTestInputPath);

  base::FilePath lock_file_path_ =
      base::PathService::CheckedGet(base::DIR_OUT_TEST_DATA_ROOT)
          .AppendASCII("link_capturing_rebaseline_lock_file.lock");

  // Current expectations for this test (parsed from the test json file).
  std::optional<base::Value> test_expectations_;
};

IN_PROC_BROWSER_TEST_P(WebAppLinkCapturingParameterizedBrowserTest,
                       CheckLinkCaptureCombinations) {
  testing::TestParamInfo<LinkCaptureTestParam> param(GetParam(), 0);

  const base::Value::Dict& test_case = GetTestCaseDataFromParam();
  if (!ShouldRunDisabledTests() &&
      test_case.FindBool("disabled").value_or(false)) {
    GTEST_SKIP()
        << "Skipped as test is marked as disabled in the expectations file. "
           "Add the switch '--run-all-tests' to run disabled tests too.";
  }

  // Install all apps.
  const webapps::AppId app_a =
      InstallTestWebApp(embedded_test_server()->GetURL(kStartPageScopeA));
  const webapps::AppId app_b =
      InstallTestWebApp(embedded_test_server()->GetURL(kDestinationPageScopeB));

  std::string element_id = GetElementId();

  // Setup the initial page.
  Browser* browser_a;
  content::WebContents* contents_a;
  {
    content::DOMMessageQueue message_queue;

    if (StartInAppWindow()) {
      auto* const proxy =
          apps::AppServiceProxyFactory::GetForProfile(profile());
      ui_test_utils::AllBrowserTabAddedWaiter waiter;
      proxy->Launch(app_a,
                    /* event_flags= */ 0, apps::LaunchSource::kFromAppListGrid);
      contents_a = waiter.Wait();
    } else {
      ASSERT_TRUE(ui_test_utils::NavigateToURL(
          browser(), embedded_test_server()->GetURL(kStartPageScopeA)));
      contents_a = browser()->tab_strip_model()->GetActiveWebContents();
    }

    std::string message;
    EXPECT_TRUE(message_queue.WaitForMessage(&message));
    EXPECT_EQ("\"ReadyForLinkCaptureTesting\"", message);

    browser_a = chrome::FindBrowserWithTab(contents_a);
    ASSERT_TRUE(browser_a != nullptr);
    ASSERT_EQ(StartInAppWindow() ? Browser::Type::TYPE_APP
                                 : Browser::Type::TYPE_NORMAL,
              browser_a->type());
  }

  {
    content::DOMMessageQueue message_queue;
    // Perform action (launch destination page).
    WebContentsCreationMonitor monitor;
    SimulateClickOnElement(contents_a, GetElementId(), GetClickMethod());

    std::string message;
    EXPECT_TRUE(message_queue.WaitForMessage(&message));
    DLOG(INFO) << message;
    std::string unquoted_message;
    ASSERT_TRUE(base::RemoveChars(message, "\"", &unquoted_message)) << message;
    EXPECT_TRUE(base::StartsWith(unquoted_message, "FinishedNavigating"))
        << unquoted_message;

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
    const base::Value::Dict* expected_state =
        test_case.FindDict("expected_state");
    ASSERT_TRUE(expected_state);
    ASSERT_EQ(*expected_state, CaptureCurrentState());
  }
}

// Pro-tip: To run only one combination from the below list, supply this...
// WebAppLinkCapturingParameterizedBrowserTest.CheckLinkCaptureCombinations/foo
// Where foo can be: AppWnd_ScopeA2A_ViaLink_LeftClick_WithOpener_TargetSelf
// See ParamToString above for possible values.
INSTANTIATE_TEST_SUITE_P(
    All,
    WebAppLinkCapturingParameterizedBrowserTest,
    testing::Combine(
        testing::Values(blink::mojom::ManifestLaunchHandler_ClientMode::kAuto),
        testing::Values(
            StartingPoint::kAppWindow,  // Starting point is app window.
            StartingPoint::kTab         // Starting point is a tab.
            ),
        testing::Values(Destination::kScopeA2A,   // Navigate in-scope A.
                        Destination::kScopeA2B),  // Navigate A -> B.
        testing::Values(RedirectType::kNone),
        testing::Values(
            NavigationElement::kElementLink,   // Navigate via element.
            NavigationElement::kElementButton  // Navigate via button.
            ),
        testing::Values(
            ClickMethod::kLeftClick,    // Simulate left-mouse click.
            ClickMethod::kMiddleClick,  // Simulate middle-mouse click.
            ClickMethod::kShiftClick    // Simulate shift click.
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
    ServiceWorker,
    WebAppLinkCapturingParameterizedBrowserTest,
    testing::Combine(
        testing::Values(blink::mojom::ManifestLaunchHandler_ClientMode::kAuto),
        testing::Values(
            StartingPoint::kAppWindow,  // Starting point is app window.
            StartingPoint::kTab         // Starting point is a tab.
            ),
        testing::Values(Destination::kScopeA2A,   // Navigate in-scope A.
                        Destination::kScopeA2B),  // Navigate A -> B.
        testing::Values(RedirectType::kNone),
        testing::Values(NavigationElement::kElementServiceWorkerButton),
        testing::Values(ClickMethod::kLeftClick),
        testing::Values(OpenerMode::kNoOpener),
        testing::Values(NavigationTarget::kBlank)),
    LinkCaptureTestParamToString);

INSTANTIATE_TEST_SUITE_P(
    Capturable,
    WebAppLinkCapturingParameterizedBrowserTest,
    testing::Combine(
        // TODO: Add kNavigateExisting.
        testing::Values(
            blink::mojom::ManifestLaunchHandler_ClientMode::kFocusExisting),
        testing::Values(StartingPoint::kAppWindow, StartingPoint::kTab),
        testing::Values(Destination::kScopeA2A,  // Navigate A -> A.
                        Destination::kScopeA2B   // Navigate A -> B.
                        ),
        // TODO: Add redirection cases.
        testing::Values(RedirectType::kNone),
        testing::Values(NavigationElement::kElementLink,
                        NavigationElement::kElementButton),
        // TODO: Add shift and middle click cases.
        testing::Values(ClickMethod::kLeftClick),
        testing::Values(OpenerMode::kNoOpener),
        testing::Values(NavigationTarget::kBlank)),
    LinkCaptureTestParamToString);

// This test verifies that there are no left-over expectations for tests that
// no longer exist in code but still exist in the expectations json file.
// Additionally if this test is run with the --rebaseline-link-capturing-test
// flag any left-over expectations will be cleaned up.
using WebAppLinkCapturingParameterizedExpectationTest =
    WebAppLinkCapturingParameterizedBrowserTest;
IN_PROC_BROWSER_TEST_F(WebAppLinkCapturingParameterizedExpectationTest,
                       CleanupExpectations) {
  std::set<std::string> test_cases;
  const testing::UnitTest* unit_test = testing::UnitTest::GetInstance();
  for (int i = 0; i < unit_test->total_test_suite_count(); ++i) {
    const testing::TestSuite* test_suite = unit_test->GetTestSuite(i);
    // We only care about link capturing parameterized tests.
    if (std::string_view(test_suite->name())
            .find("WebAppLinkCapturingParameterizedBrowserTest") ==
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
        << "Run this test with --rebaseline-link-capturing-test to clean this "
           "up.";
  }
}
