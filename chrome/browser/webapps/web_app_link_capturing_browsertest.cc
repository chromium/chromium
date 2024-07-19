// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/types/expected.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
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
constexpr char kValueLeftClick[] = "LEFT";
constexpr char kValueMiddleClick[] = "MIDDLE";
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

enum LinkCaptureTestParam {
  kInvalid = 0,
  // The starting point for the test:
  kAppWindow = 1 << 1,
  kTab = 1 << 2,
  // Whether to navigate within the same scope or outside it:
  kScopeA2A = 1 << 3,
  kScopeA2B = 1 << 4,
  kScopeA2ARedirectB = 1 << 5,
  kScopeA2BRedirectA = 1 << 6,
  // The element to use for navigation:
  kElementLink = 1 << 7,
  kElementButton = 1 << 8,
  // The method of interacting with the element:
  kLeftClick = 1 << 9,
  kMiddleClick = 1 << 10,
  // Whether to supply an Opener/NoOpener:
  kOpener = 1 << 11,
  kNoOpener = 1 << 12,
  // The target to supply for the navigation:
  kSelf = 1 << 13,
  kFrame = 1 << 14,
  kBlank = 1 << 15,
  kNoFrame = 1 << 16,
};

// This helper class monitors WebContents creation in all tabs (of all browsers)
// and can be queried for the last one seen.
class WebContentsCreationMonitor : public ui_test_utils::AllTabsObserver {
 public:
  WebContentsCreationMonitor() { AddAllBrowsers(); }
  ~WebContentsCreationMonitor() override { last_seen_web_contents_ = nullptr; }

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
// --gtest_filter=*WebAppLinkCapturingBrowserTestParameterized.* \
// --rebaseline-link-capturing-test
//
class WebAppLinkCapturingBrowserTestParameterized
    : public InProcessBrowserTest,
      public testing::WithParamInterface<std::tuple<LinkCaptureTestParam,
                                                    LinkCaptureTestParam,
                                                    LinkCaptureTestParam,
                                                    LinkCaptureTestParam,
                                                    LinkCaptureTestParam,
                                                    LinkCaptureTestParam>> {
 public:
  WebAppLinkCapturingBrowserTestParameterized() {
    auto link_capture_test_path = GetPathForLinkCaptureInputJson();
    CHECK(link_capture_test_path.has_value())
        << " Unable to parse link capture file for testing";
    json_file_path_ = link_capture_test_path.value();

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

  // This function converts the test param to a string, which is used to provide
  // a unique for the given test run.
  static std::string ParamToString(
      testing::TestParamInfo<std::tuple<LinkCaptureTestParam,
                                        LinkCaptureTestParam,
                                        LinkCaptureTestParam,
                                        LinkCaptureTestParam,
                                        LinkCaptureTestParam,
                                        LinkCaptureTestParam>> param_info) {
    std::string result = "";

    LinkCaptureTestParam container = std::get<0>(param_info.param);
    if (container & LinkCaptureTestParam::kAppWindow) {
      result += "AppWnd_";
    }
    if (container & LinkCaptureTestParam::kTab) {
      result += "Tab_";
    }

    LinkCaptureTestParam destination = std::get<1>(param_info.param);
    if (destination & LinkCaptureTestParam::kScopeA2A) {
      result += "ScopeA2A_";
    }
    if (destination & LinkCaptureTestParam::kScopeA2B) {
      result += "ScopeA2B_";
    }
    if (destination & LinkCaptureTestParam::kScopeA2ARedirectB) {
      result += "ScopeA2ARedirectB_";
    }
    if (destination & LinkCaptureTestParam::kScopeA2BRedirectA) {
      result += "ScopeA2BRedirectA_";
    }

    LinkCaptureTestParam element = std::get<2>(param_info.param);
    if (element & LinkCaptureTestParam::kElementLink) {
      result += "ViaLink_";
    }
    if (element & LinkCaptureTestParam::kElementButton) {
      result += "ViaButton_";
    }

    LinkCaptureTestParam method = std::get<3>(param_info.param);
    if (method & LinkCaptureTestParam::kLeftClick) {
      result += "LeftClick_";
    }
    if (method & LinkCaptureTestParam::kMiddleClick) {
      result += "MiddleClick_";
    }

    LinkCaptureTestParam opener = std::get<4>(param_info.param);
    if (opener & LinkCaptureTestParam::kOpener) {
      result += "WithOpener_";
    }
    if (opener & LinkCaptureTestParam::kNoOpener) {
      result += "WithoutOpener_";
    }

    LinkCaptureTestParam target = std::get<5>(param_info.param);
    if (target & LinkCaptureTestParam::kSelf) {
      result += "TargetSelf";
    }
    if (target & LinkCaptureTestParam::kFrame) {
      result += "TargetFrame";
    }
    if (target & LinkCaptureTestParam::kBlank) {
      result += "TargetBlank";
    }
    if (target & LinkCaptureTestParam::kNoFrame) {
      result += "TargetNoFrame";
    }

    return result;
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
  TestExpectation GetTestExpectationFromParam() {
    testing::TestParamInfo<std::tuple<
        LinkCaptureTestParam, LinkCaptureTestParam, LinkCaptureTestParam,
        LinkCaptureTestParam, LinkCaptureTestParam, LinkCaptureTestParam>>
        param(GetParam(), 0);

    const base::Value& value = test_expectations_.value();
    const base::Value::Dict& dict = value.GetDict();
    const base::Value::List* list = dict.FindList("expectations");
    for (const auto& entry : *list) {
      const base::Value::Dict& log_entry = entry.GetDict();

      const std::string* start = log_entry.FindString("start");
      const std::string* scope = log_entry.FindString("scope");
      const std::string* element = log_entry.FindString("element");
      const std::string* click = log_entry.FindString("click");
      const std::string* opener = log_entry.FindString("opener");
      const std::string* target = log_entry.FindString("target");

      if (start->compare(StartInAppWindow() ? kValueApp : kValueTab) != 0) {
        continue;
      }

      if ((WillNavigateA2A() && scope->compare(kValueScopeA2A) != 0) ||
          (WillNavigateA2B() && scope->compare(kValueScopeA2B) != 0) ||
          (WillNavigateA2AWithRedir() &&
           scope->compare(kValueScopeA2ARedirectB) != 0) ||
          (WillNavigateA2BWithRedir() &&
           scope->compare(kValueScopeA2BRedirectA) != 0)) {
        continue;
      }

      if (element->compare(WillNavigateViaLink() ? kValueLink : kValueButton) !=
          0) {
        continue;
      }

      if (click->compare(IsMiddleClick() ? kValueMiddleClick
                                         : kValueLeftClick) != 0) {
        continue;
      }

      if (opener->compare(WithOpener() ? kValueOpener : kValueNoOpener) != 0) {
        continue;
      }

      if ((IsTargetSelf() && target->compare(kValueTargetSelf) != 0) ||
          (IsTargetFrame() && target->compare(kValueTargetFrame) != 0) ||
          (IsTargetBlank() && target->compare(kValueTargetBlank) != 0) ||
          (IsTargetNoFrame() && target->compare(kValueTargetNoFrame) != 0)) {
        continue;
      }

      std::string expectation = *log_entry.FindString("expect");
      std::vector<std::string> tokens = base::SplitString(
          expectation, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      TestExpectation test_expectation = {
          Browser::Type::TYPE_PICTURE_IN_PICTURE, false, false};
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

    NOTREACHED() << "Missing expectation in test file";
  }

  // This function runs a javascript on the `contents`, which will result in a
  // click to `element_id` being simulated. Set `middle_click` to `true` to
  // change from the default behavior (which is to left-click). Returns `true`
  // if successful, but false when an error occurs (see dev console or execution
  // log).
  bool SimulateClickOnElement(content::WebContents* contents,
                              std::string element_id,
                              bool middle_click) {
    std::string properties = middle_click ? "{ctrlKey: true}" : "{}";
    std::string js =
        "simulateClick(\"" + element_id + "\", " + properties + ")";
    return ExecJs(contents, js);
  }

  // This function is used during rebaselining to record (to a file) the results
  // from an actual run of a single test case. Constructs a json dictionary and
  // saves it to the test results json file. Returns true if writing was
  // successful.
  bool RecordActualResults(Browser::Type type,
                           bool same_browser_instance,
                           bool in_iframe) {
    std::string input_template =
        "{\"start\": \"$1\", \"scope\": \"$2\", \"element\": \"$3\", "
        "\"click\": \"$4\", \"opener\": \"$5\", \"target\": \"$6\", "
        "\"expect\": \"$7\"}";

    std::vector<std::string> substitutions;
    substitutions.push_back(StartInAppWindow() ? kValueApp : kValueTab);
    std::string scope = "invalid-scope";
    if (WillNavigateA2A()) {
      scope = kValueScopeA2A;
    } else if (WillNavigateA2B()) {
      scope = kValueScopeA2B;
    } else if (WillNavigateA2AWithRedir()) {
      scope = kValueScopeA2ARedirectB;
    } else if (WillNavigateA2BWithRedir()) {
      scope = kValueScopeA2BRedirectA;
    }
    substitutions.push_back(scope);
    substitutions.push_back(WillNavigateViaLink() ? kValueLink : kValueButton);
    substitutions.push_back(IsMiddleClick() ? kValueMiddleClick
                                            : kValueLeftClick);
    substitutions.push_back(WithOpener() ? kValueOpener : kValueNoOpener);
    std::string target = "invalid-target";
    if (IsTargetSelf()) {
      target = kValueTargetSelf;
    } else if (IsTargetFrame()) {
      target = kValueTargetFrame;
    } else if (IsTargetBlank()) {
      target = kValueTargetBlank;
    } else if (IsTargetNoFrame()) {
      target = kValueTargetNoFrame;
    }
    substitutions.push_back(target);
    std::string expect =
        BrowserTypeToString(type) + " " +
        (same_browser_instance ? kValueSameBrowser : kValueOtherBrowser) + " " +
        (in_iframe ? kValueInIFrame : kValueInMain);
    substitutions.push_back(expect);

    std::string output =
        base::ReplaceStringPlaceholders(input_template, substitutions, nullptr);

    const ::testing::TestInfo* test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    std::string test_name = test_info->name();
    // Using the test name to figure out what is the first test in the series is
    // not ideal, but it gets the job done. A better way would be to query the
    // test for the index/name of the first and last test, but all I can find is
    // how many tests there are total in this run, but not what the index is for
    // the current test.
    bool first_run =
        test_name.find(
            "AppWnd_ScopeA2A_ViaLink_LeftClick_WithOpener_TargetSelf") !=
        std::string::npos;
    bool last_run = test_name.find(
                        "Tab_ScopeA2BRedirectA_ViaButton_MiddleClick_"
                        "WithoutOpener_TargetNoFrame") != std::string::npos;

    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      if (first_run) {
        output = "{\"expectations\": [\n" + output;
        return base::WriteFile(json_file_path_, output);
      } else {
        // Add trailing comma to last entry and write a new entry.
        output = ",\n" + output;
        if (last_run) {
          // Wrap up the rest of the json.
          output += "\n]}";
        }
        base::File file(json_file_path_,
                        base::File::FLAG_OPEN | base::File::FLAG_APPEND);
        return file.Write(-1, output.c_str(), output.length()) ==
               (int)output.length();
      }
    }
  }

  // Returns `true` if the test should start inside an app window (and `false`
  // if the test should start in a tab).
  bool StartInAppWindow() const {
    LinkCaptureTestParam container = std::get<0>(GetParam());
    return (container & LinkCaptureTestParam::kAppWindow);
  }

  // Returns `true` if the test should navigate to a page within the same scope.
  bool WillNavigateA2A() const {
    LinkCaptureTestParam scope = std::get<1>(GetParam());
    return (scope & LinkCaptureTestParam::kScopeA2A);
  }

  // Returns `true` if the test should navigate to a page in a different scope.
  bool WillNavigateA2B() const {
    LinkCaptureTestParam scope = std::get<1>(GetParam());
    return (scope & LinkCaptureTestParam::kScopeA2B);
  }

  // Returns `true` if the test should navigate to a page in a different scope,
  // but end up on the same scope due to an HTTP redirect.
  bool WillNavigateA2AWithRedir() const {
    LinkCaptureTestParam scope = std::get<1>(GetParam());
    return (scope & LinkCaptureTestParam::kScopeA2ARedirectB);
  }

  // Returns `true` if the test should navigate to a page in the same scope, but
  // end up on back scope A due to an HTTP redirect.
  bool WillNavigateA2BWithRedir() const {
    LinkCaptureTestParam scope = std::get<1>(GetParam());
    return (scope & LinkCaptureTestParam::kScopeA2BRedirectA);
  }

  // Returns `true` if the test should use a link to navigate (and `false` if
  // the test should use a button).
  bool WillNavigateViaLink() const {
    LinkCaptureTestParam element = std::get<2>(GetParam());
    return (element & LinkCaptureTestParam::kElementLink);
  }

  // Returns `true` if the test should use a middle-click for the navigation
  // click (and `false` if the test should use left-click).
  bool IsMiddleClick() const {
    LinkCaptureTestParam method = std::get<3>(GetParam());
    return (method & LinkCaptureTestParam::kMiddleClick);
  }

  // Returns `true` if the test should supply an opener value.
  bool WithOpener() const {
    LinkCaptureTestParam opener = std::get<4>(GetParam());
    return (opener & LinkCaptureTestParam::kOpener);
  }

  // Returns `true` if the test should target _self for the navigation.
  bool IsTargetSelf() const {
    LinkCaptureTestParam target = std::get<5>(GetParam());
    return (target & LinkCaptureTestParam::kSelf);
  }

  // Returns `true` if the test should target a named frame for the navigation.
  bool IsTargetFrame() const {
    LinkCaptureTestParam target = std::get<5>(GetParam());
    return (target & LinkCaptureTestParam::kFrame);
  }

  // Returns `true` if the test should target _blank for the navigation.
  bool IsTargetBlank() const {
    LinkCaptureTestParam target = std::get<5>(GetParam());
    return (target & LinkCaptureTestParam::kBlank);
  }

  // Returns `true` if the test should target _ for the navigation.
  bool IsTargetNoFrame() const {
    LinkCaptureTestParam target = std::get<5>(GetParam());
    return (target & LinkCaptureTestParam::kNoFrame);
  }

  // The test page contains elements (links and buttons) that are configured
  // for each combination. This function obtains the right element id to use
  // in the navigation click.
  std::string GetElementId() {
    std::string id = "id-";

    if (WillNavigateViaLink()) {
      id += kValueLink;
    } else {
      id += kValueButton;
    }
    id += "-";
    if (WillNavigateA2A() || WillNavigateA2AWithRedir()) {
      id += kValueScopeA2A;
    } else if (WillNavigateA2B() || WillNavigateA2BWithRedir()) {
      id += kValueScopeA2B;
    }

    id += "-";
    if (IsTargetSelf()) {
      id += kValueTargetSelf;
    }
    if (IsTargetFrame()) {
      id += kValueTargetFrame;
    }
    if (IsTargetBlank()) {
      id += kValueTargetBlank;
    }
    if (IsTargetNoFrame()) {
      id += kValueTargetNoFrame;
    }

    id += "-";
    if (WithOpener()) {
      id += kValueOpener;
    } else {
      id += kValueNoOpener;
    }

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
    if (ShouldRebaseline()) {
      // Use a dummy expectation file while rebaselining (see function comment).
      json_data = R"(
        {"expectations": [
        {
          "start": "APP",
          "scope": "SAME",
          "element": "LINK",
          "click": "MIDDLE",
          "opener": "NO_OPENER",
          "target": "NO_FRAME",
          "expect": "TYPE_NORMAL SAME_BROWSER IFRAME"
        }]}
      )";
    } else {
      ASSERT_TRUE(ReadFileToString(json_file_path_, &json_data));
    }
    test_expectations_ = base::JSONReader::Read(json_data);
    ASSERT_TRUE(test_expectations_) << "Unable to read test expectation file";
    ASSERT_TRUE(test_expectations_.value().is_dict());
  }

  // The path to the json file containing the test expectations.
  base::FilePath json_file_path_;

  // Current expectations for this test (parsed from the test json file).
  std::optional<base::Value> test_expectations_;

  // static int test_case_counter_;
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
};

// Intentionally disabled -- this can be enabled manually on Linux to verify
// link capturing use-cases. Expectations for other platforms might be different
// and need to be generated separately.
IN_PROC_BROWSER_TEST_P(WebAppLinkCapturingBrowserTestParameterized,
                       DISABLED_CheckLinkCaptureCombinations) {
  testing::TestParamInfo<std::tuple<LinkCaptureTestParam, LinkCaptureTestParam,
                                    LinkCaptureTestParam, LinkCaptureTestParam,
                                    LinkCaptureTestParam, LinkCaptureTestParam>>
      param(GetParam(), 0);

  // Use PiP browser type as default because it would always be an unexpected
  // result for this test.
  TestExpectation expectation = {Browser::Type::TYPE_PICTURE_IN_PICTURE, false,
                                 false};
  if (!ShouldRebaseline()) {
    expectation = GetTestExpectationFromParam();
  }

  std::string element_id = GetElementId();

  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &WebAppLinkCapturingBrowserTestParameterized::SimulateRedirectHandler,
      base::Unretained(this)));
  ASSERT_TRUE(embedded_test_server()->Start());

  std::string trace =
      std::string("\n---------------------------\nParameterized test: ") +
      "Test name: " + ParamToString(param) +
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

  // Setup links for scope B (unless we're staying in scope A the whole time).
  if (!WillNavigateA2A()) {
    GURL url = embedded_test_server()->GetURL(kDestinationPageScopeB);
    const webapps::AppId app_b = InstallTestWebApp(url);

    std::string js = std::string("setLinksForScopeB('") + url.spec().c_str() +
                     "', 'target', 'noopener')";
    ASSERT_TRUE(ExecJs(contents_a, js));
  }

  content::WebContents* contents_b;
  bool in_iframe = false;
  {
    content::DOMMessageQueue message_queue;

    // Perform action (launch destination page).
    WebContentsCreationMonitor monitor;
    ASSERT_TRUE(
        SimulateClickOnElement(contents_a, GetElementId(), IsMiddleClick()));

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
    ASSERT_EQ(true, contents_b->GetURL().is_valid());
  }

  Browser* browser_b = ToBrowser(contents_b);
  ASSERT_TRUE(browser_b != nullptr);
  Browser::Type browser_type_b = browser_b->type();

  if (ShouldRebaseline()) {
    ASSERT_TRUE(
        RecordActualResults(browser_type_b, browser_a == browser_b, in_iframe));
  } else {
    // Make sure browser type and browser creation match expectations.
    ASSERT_EQ(BrowserTypeToString(expectation.browser_type),
              BrowserTypeToString(browser_type_b));
    ASSERT_EQ(expectation.same_browser, browser_a == browser_b);
    ASSERT_EQ(expectation.in_iframe, in_iframe);
  }
}

// Pro-tip: To run only one combination from the below list, supply this...
// WebAppLinkCapturingBrowserTestParameterized.CheckLinkCaptureCombinations/foo
// Where foo can be: AppWnd_ScopeA2A_ViaLink_LeftClick_WithOpener_TargetSelf
// See ParamToString above for possible values.
INSTANTIATE_TEST_SUITE_P(
    All,
    WebAppLinkCapturingBrowserTestParameterized,
    testing::Combine(
        testing::Values(
            LinkCaptureTestParam::kAppWindow,  // Starting point is app window.
            LinkCaptureTestParam::kTab         // Starting point is a tab.
            ),
        testing::Values(
            LinkCaptureTestParam::kScopeA2A,           // Navigate in-scope A.
            LinkCaptureTestParam::kScopeA2B,           // Navigate A -> B.
            LinkCaptureTestParam::kScopeA2ARedirectB,  // Redirect A -> B.
            LinkCaptureTestParam::kScopeA2BRedirectA   // Redirect back to A.
            ),
        testing::Values(
            LinkCaptureTestParam::kElementLink,   // Navigate via element.
            LinkCaptureTestParam::kElementButton  // Navigate via button.
            ),
        testing::Values(
            LinkCaptureTestParam::kLeftClick,   // Simulate left-mouse click.
            LinkCaptureTestParam::kMiddleClick  // Simulate middle-mouse click
            ),
        testing::Values(
            LinkCaptureTestParam::kOpener,   // Supply 'opener' property.
            LinkCaptureTestParam::kNoOpener  // Supply 'noopener' property.
            ),
        testing::Values(
            LinkCaptureTestParam::kSelf,    // Use target _self.
            LinkCaptureTestParam::kFrame,   // Use named frame as target.
            LinkCaptureTestParam::kBlank,   // User Target is _blank.
            LinkCaptureTestParam::kNoFrame  // Target is non-existing frame.
            )),
    WebAppLinkCapturingBrowserTestParameterized::ParamToString);
