// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/types/expected.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_permission_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/window_open_disposition.h"
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

constexpr char kValueApp[] = "APP";
constexpr char kValueTab[] = "TAB";
constexpr char kValueScopeA2A[] = "A_TO_A";
constexpr char kValueScopeA2B[] = "A_TO_B";
constexpr char kValueScopeA2ARedirectB[] = "A_TO_A->B";
constexpr char kValueScopeA2BRedirectA[] = "A_TO_B->A";
constexpr char kValueLink[] = "LINK";
constexpr char kValueButton[] = "BTN";
constexpr char kValueServiceWorkerButton[] = "BTN_SW";
constexpr char kValueLeftClick[] = "LEFT";
constexpr char kValueMiddleClick[] = "MIDDLE";
constexpr char kValueShiftClick[] = "SHIFT";
constexpr char kValueOpener[] = "OPENER";
constexpr char kValueNoOpener[] = "NO_OPENER";
constexpr char kValueTargetSelf[] = "SELF";
constexpr char kValueTargetFrame[] = "FRAME";
constexpr char kValueTargetBlank[] = "BLANK";
constexpr char kValueTargetNoFrame[] = "NO_FRAME";
constexpr char kValueSameBrowser[] = "SAME_BROWSER";
constexpr char kValueOtherBrowser[] = "OTHER_BROWSER";
constexpr char kValueInIFrame[] = "IN_IFRAME";
constexpr char kValueInMain[] = "IN_MAIN";

// The starting point for the test:
enum class StartingPoint {
  kAppWindow,
  kTab,
};

std::string ToJsonString(StartingPoint start) {
  switch (start) {
    case StartingPoint::kAppWindow:
      return kValueApp;
    case StartingPoint::kTab:
      return kValueTab;
  }
}

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
  kScopeA2ARedirectB,
  kScopeA2BRedirectA,
};

std::string ToJsonString(Destination scope) {
  switch (scope) {
    case Destination::kScopeA2A:
      return kValueScopeA2A;
    case Destination::kScopeA2B:
      return kValueScopeA2B;
    case Destination::kScopeA2ARedirectB:
      return kValueScopeA2ARedirectB;
    case Destination::kScopeA2BRedirectA:
      return kValueScopeA2BRedirectA;
  }
}

std::string_view ToParamString(Destination scope) {
  switch (scope) {
    case Destination::kScopeA2A:
      return "ScopeA2A";
    case Destination::kScopeA2B:
      return "ScopeA2B";
    case Destination::kScopeA2ARedirectB:
      return "ScopeA2ARedirectB";
    case Destination::kScopeA2BRedirectA:
      return "ScopeA2BRedirectA";
  }
}

// The element to use for navigation:
enum class NavigationElement {
  kElementLink,
  kElementButton,
  kElementServiceWorkerButton,
};

std::string ToJsonString(NavigationElement element) {
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

std::string ToJsonString(ClickMethod click) {
  switch (click) {
    case ClickMethod::kLeftClick:
      return kValueLeftClick;
    case ClickMethod::kMiddleClick:
      return kValueMiddleClick;
    case ClickMethod::kShiftClick:
      return kValueShiftClick;
  }
}

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

std::string ToJsonString(OpenerMode opener) {
  switch (opener) {
    case OpenerMode::kOpener:
      return kValueOpener;
    case OpenerMode::kNoOpener:
      return kValueNoOpener;
  }
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

std::string ToJsonString(NavigationTarget target) {
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
using LinkCaptureTestParam = std::tuple<StartingPoint,
                                        Destination,
                                        NavigationElement,
                                        ClickMethod,
                                        OpenerMode,
                                        NavigationTarget>;

std::string LinkCaptureTestParamToString(
    testing::TestParamInfo<LinkCaptureTestParam> param_info) {
  // Concatenates the result of calling `ToParamString()` on each member of the
  // tuple with '_' in between fields.
  return std::apply(
      [](auto&... p) { return base::JoinString({ToParamString(p)...}, "_"); },
      param_info.param);
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
// If link capturing behavior changes, the test expectations would need to be
// updated. This can be done manually (by editing the json file directly), or it
// can be done automatically by using the flag --rebaseline-link-capturing-test.
//
// Example usage:
// out/Default/browser_tests \
// --gtest_filter=*WebAppLinkCapturingParameterizedBrowserTest.* \
// --rebaseline-link-capturing-test
//
class WebAppLinkCapturingParameterizedBrowserTest
    : public InProcessBrowserTest,
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
    if (!WillNavigateA2AWithRedir() && !WillNavigateA2BWithRedir()) {
      return nullptr;  // This test is not using redirects.
    }
    if (request.GetURL().spec().find("/destination.html") ==
        std::string::npos) {
      return nullptr;  // Only redirect for destination pages.
    }

    GURL redirect_from = embedded_test_server()->GetURL(
        WillNavigateA2AWithRedir() ? kDestinationPageScopeA
                                   : kDestinationPageScopeB);
    GURL redirect_to = embedded_test_server()->GetURL(
        WillNavigateA2AWithRedir() ? kDestinationPageScopeB
                                   : kDestinationPageScopeA);

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
  struct TestExpectation {
    Browser::Type browser_type;
    bool same_browser;
    bool in_iframe;
  };

  // Obtains expected results for the current test run. Returned as a pair of
  // (expected) BrowserType and a bool signifying whether the test should expect
  // the old browser object to be used for the navigation to the destination.

  base::Value::Dict& GetExpectationValueFromParam() {
    base::Value& value = test_expectations_.value();
    base::Value::Dict& dict = value.GetDict();
    base::Value::List* list = dict.EnsureList("expectations");

    for (base::Value& entry : *list) {
      base::Value::Dict& log_entry = entry.GetDict();

      const std::string* start = log_entry.FindString("start");
      const std::string* scope = log_entry.FindString("scope");
      const std::string* element = log_entry.FindString("element");
      const std::string* click = log_entry.FindString("click");
      const std::string* opener = log_entry.FindString("opener");
      const std::string* target = log_entry.FindString("target");

      if (!start || *start != ToJsonString(GetStartingPoint())) {
        continue;
      }

      if (!scope || *scope != ToJsonString(GetDestination())) {
        continue;
      }

      if (!element || *element != ToJsonString(GetNavigationElement())) {
        continue;
      }

      if (!click || *click != ToJsonString(GetClickMethod())) {
        continue;
      }

      if (!opener || *opener != ToJsonString(GetOpenerMode())) {
        continue;
      }

      if (!target || *target != ToJsonString(GetNavigationTarget())) {
        continue;
      }

      return log_entry;
    }

    base::Value::Dict new_expectation =
        base::Value::Dict()
            .Set("start", ToJsonString(GetStartingPoint()))
            .Set("scope", ToJsonString(GetDestination()))
            .Set("element", ToJsonString(GetNavigationElement()))
            .Set("click", ToJsonString(GetClickMethod()))
            .Set("opener", ToJsonString(GetOpenerMode()))
            .Set("target", ToJsonString(GetNavigationTarget()));
    list->Append(std::move(new_expectation));
    return list->back().GetDict();
  }

  TestExpectation GetTestExpectationFromParam() {
    const base::Value::Dict& log_entry = GetExpectationValueFromParam();
    const std::string* expectation = log_entry.FindString("expect");
    CHECK(expectation) << "Missing expectation in test file";
    std::vector<std::string> tokens = base::SplitString(
        *expectation, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    TestExpectation test_expectation = {Browser::Type::TYPE_PICTURE_IN_PICTURE,
                                        false, false};
    for (size_t i = 0; i < tokens.size(); ++i) {
      if (i == 0) {
        test_expectation.browser_type = StringToBrowserType(tokens[0]);
      } else if (tokens[i].compare(kValueSameBrowser) == 0) {
        test_expectation.same_browser = true;
      } else if (tokens[i].compare(kValueInIFrame) == 0) {
        test_expectation.in_iframe = true;
      }
    }
    return test_expectation;
  }

  // This function runs a javascript on the `contents`, which will result in a
  // click to `element_id` being simulated. Set `middle_click` to `true` to
  // change from the default behavior (which is to left-click). Returns `true`
  // if successful, but false when an error occurs (see dev console or execution
  // log).
  bool SimulateClickOnElement(content::WebContents* contents,
                              std::string element_id,
                              ClickMethod click) {
    auto GetClickProperties = [](ClickMethod click) -> std::string {
      switch (click) {
        case ClickMethod::kLeftClick:
          return "{}";
        case ClickMethod::kMiddleClick:
          return "{ctrlKey: true}";
        case ClickMethod::kShiftClick:
          return "{shiftKey: true}";
      }
    };
    std::string properties = GetClickProperties(click);
    std::string js =
        "simulateClick(\"" + element_id + "\", " + properties + ")";
    return ExecJs(contents, js);
  }

  // This function is used during rebaselining to record (to a file) the results
  // from an actual run of a single test case. Constructs a json dictionary and
  // saves it to the test results json file. Returns true if writing was
  // successful.
  void RecordActualResults(Browser::Type type,
                           bool same_browser_instance,
                           bool in_iframe) {
    std::string expect =
        BrowserTypeToString(type) + " " +
        (same_browser_instance ? kValueSameBrowser : kValueOtherBrowser) + " " +
        (in_iframe ? kValueInIFrame : kValueInMain);

    base::Value::Dict& expectation = GetExpectationValueFromParam();
    expectation.Set("expect", expect);
    SaveExpectations();
  }

  void SaveExpectations() {
    // Sort the list of test cases, ignoring the actual expectation.
    base::ranges::sort(*test_expectations_->GetDict().FindList("expectations"),
                       /*comp=*/{}, /*proj=*/[](const base::Value& v) {
                         base::Value::Dict dict = v.GetDict().Clone();
                         dict.Remove("expect");
                         return base::Value(std::move(dict));
                       });
    // And write formatted JSON back to disk.
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      std::optional<std::string> json_string = base::WriteJsonWithOptions(
          *test_expectations_, base::JsonOptions::OPTIONS_PRETTY_PRINT);
      ASSERT_TRUE(json_string.has_value());
      ASSERT_TRUE(base::WriteFile(json_file_path_, *json_string));
    }
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

  // Returns `true` if the test should navigate to a page within the same scope.
  bool WillNavigateA2A() const {
    return GetDestination() == Destination::kScopeA2A;
  }

  // Returns `true` if the test should navigate to a page in a different scope.
  bool WillNavigateA2B() const {
    return GetDestination() == Destination::kScopeA2B;
  }

  // Returns `true` if the test should navigate to a page in a different scope,
  // but end up on the same scope due to an HTTP redirect.
  bool WillNavigateA2AWithRedir() const {
    return GetDestination() == Destination::kScopeA2ARedirectB;
  }

  // Returns `true` if the test should navigate to a page in the same scope, but
  // end up on back scope A due to an HTTP redirect.
  bool WillNavigateA2BWithRedir() const {
    return GetDestination() == Destination::kScopeA2BRedirectA;
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
    std::string id = "id-";

    id += ToJsonString(GetNavigationElement());
    id += "-";
    if (WillNavigateA2A() || WillNavigateA2AWithRedir()) {
      id += kValueScopeA2A;
    } else if (WillNavigateA2B() || WillNavigateA2BWithRedir()) {
      id += kValueScopeA2B;
    }

    id += "-";
    id += ToJsonString(GetNavigationTarget());

    id += "-";
    id += ToJsonString(GetOpenerMode());

    return id;
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

  webapps::AppId InstallTestWebApp(const GURL& start_url) {
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->user_display_mode =
        web_app::mojom::UserDisplayMode::kStandalone;
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

  Browser* ToBrowser(content::WebContents* web_contents) {
    gfx::NativeWindow native_window = web_contents->GetTopLevelNativeWindow();
    return chrome::FindBrowserWithWindow(native_window);
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

  // Parses the json test expectation file. Note that during rebaselining, a
  // dummy json file is used, because the json test expectation file is still
  // being constructed and likely contains invalid values.
  void InitializeTestExpectations() {
    std::string json_data;
    bool success = ReadFileToString(json_file_path_, &json_data);
    if (!ShouldRebaseline()) {
      ASSERT_TRUE(success) << "Failed to read test baselines";
    }
    if (!success) {
      json_data = R"(
          {"expectations": []}
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

  // Current expectations for this test (parsed from the test json file).
  std::optional<base::Value> test_expectations_;

  // static int test_case_counter_;
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
};

// Intentionally disabled -- this can be enabled manually on Linux to verify
// link capturing use-cases. Expectations for other platforms might be different
// and need to be generated separately.
IN_PROC_BROWSER_TEST_P(WebAppLinkCapturingParameterizedBrowserTest,
                       DISABLED_CheckLinkCaptureCombinations) {
  testing::TestParamInfo<LinkCaptureTestParam> param(GetParam(), 0);

  // Use PiP browser type as default because it would always be an unexpected
  // result for this test.
  TestExpectation expectation = {Browser::Type::TYPE_PICTURE_IN_PICTURE, false,
                                 false};
  if (!ShouldRebaseline()) {
    expectation = GetTestExpectationFromParam();
  }

  std::string element_id = GetElementId();

  std::string trace =
      std::string("\n---------------------------\nParameterized test: ") +
      "Test name: " + LinkCaptureTestParamToString(param) +
      "\n"
      "clicking : " +
      element_id + " " +
      (ShouldRebaseline()
           ? "Rebaseline in progress "
           : "Expect: " + BrowserTypeToString(expectation.browser_type) + " " +
                 (expectation.same_browser ? "SAME_BROWSER" : "OTHER_BROWSER") +
                 " " + (expectation.in_iframe ? "IN_IFRAME" : "IN_MAIN"));

  SCOPED_TRACE(trace);

  // Setup the initial page.
  Browser* browser_a;
  content::WebContents* contents_a;
  {
    content::DOMMessageQueue message_queue;

    if (StartInAppWindow()) {
      // Setup the starting app.
      const webapps::AppId app_a =
          InstallTestWebApp(embedded_test_server()->GetURL(kStartPageScopeA));

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

    browser_a = ToBrowser(contents_a);
    ASSERT_TRUE(browser_a != nullptr);
    ASSERT_EQ(StartInAppWindow() ? Browser::Type::TYPE_APP
                                 : Browser::Type::TYPE_NORMAL,
              browser_a->type());
  }

  // Install app 'B' if required.
  if (!WillNavigateA2A()) {
    InstallTestWebApp(embedded_test_server()->GetURL(kDestinationPageScopeB));
  }

  content::WebContents* contents_b;
  bool in_iframe = false;
  {
    content::DOMMessageQueue message_queue;

    // Perform action (launch destination page).
    WebContentsCreationMonitor monitor;
    ASSERT_TRUE(
        SimulateClickOnElement(contents_a, GetElementId(), GetClickMethod()));
    std::string message;
    EXPECT_TRUE(message_queue.WaitForMessage(&message));
    std::string unquoted_message;
    ASSERT_TRUE(base::RemoveChars(message, "\"", &unquoted_message));
    std::vector parts =
        base::SplitString(unquoted_message, ":", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    EXPECT_EQ("FinishedNavigating in frame", parts[0]);
    in_iframe = parts[1].compare("iframe") == 0;

    contents_b = monitor.GetLastSeenWebContentsAndStopMonitoring();

    ASSERT_NE(nullptr, contents_b);
    ASSERT_TRUE(contents_b->GetURL().is_valid());
  }

  Browser* browser_b = ToBrowser(contents_b);
  ASSERT_NE(browser_b, nullptr);
  Browser::Type browser_type_b = browser_b->type();

  if (ShouldRebaseline()) {
    RecordActualResults(browser_type_b, browser_a == browser_b, in_iframe);
  } else {
    // Make sure browser type and browser creation match expectations.
    ASSERT_EQ(BrowserTypeToString(expectation.browser_type),
              BrowserTypeToString(browser_type_b));
    ASSERT_EQ(expectation.same_browser, browser_a == browser_b);
    ASSERT_EQ(expectation.in_iframe, in_iframe);
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
        testing::Values(
            StartingPoint::kAppWindow,  // Starting point is app window.
            StartingPoint::kTab         // Starting point is a tab.
            ),
        testing::Values(Destination::kScopeA2A,  // Navigate in-scope A.
                        Destination::kScopeA2B,  // Navigate A -> B.
                        Destination::kScopeA2ARedirectB,  // Redirect A -> B.
                        Destination::kScopeA2BRedirectA   // Redirect back to A.
                        ),
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
        testing::Values(
            StartingPoint::kAppWindow,  // Starting point is app window.
            StartingPoint::kTab         // Starting point is a tab.
            ),
        testing::Values(Destination::kScopeA2A,  // Navigate in-scope A.
                        Destination::kScopeA2B,  // Navigate A -> B.
                        Destination::kScopeA2ARedirectB,  // Redirect A -> B.
                        Destination::kScopeA2BRedirectA   // Redirect back to A.
                        ),
        testing::Values(NavigationElement::kElementServiceWorkerButton),
        testing::Values(ClickMethod::kLeftClick),
        testing::Values(OpenerMode::kNoOpener),
        testing::Values(NavigationTarget::kBlank)),
    LinkCaptureTestParamToString);
