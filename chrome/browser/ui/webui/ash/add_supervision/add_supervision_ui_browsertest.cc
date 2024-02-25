// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/add_supervision/add_supervision_metrics_recorder.h"
#include "chrome/browser/ui/webui/ash/add_supervision/add_supervision_ui.h"
#include "chrome/browser/ui/webui/ash/add_supervision/confirm_signout_dialog.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/network_connection_change_simulator.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {
// Element references for use in test scripts.
const char kGetAddSupervisionAppElementJS[] =
    "document.querySelector('add-supervision-app')";

const char kGetAddSupervisionUIElementJS[] =
    "document.querySelector('add-supervision-app')"
    ".shadowRoot.querySelector('add-supervision-ui')";

const char kGetSupervisedUserOfflineElementJS[] =
    "document.querySelector('add-supervision-app')"
    ".shadowRoot.querySelector('supervised-user-offline')";

const char kGetSupervisedUserErrorElementJS[] =
    "document.querySelector('add-supervision-app')"
    ".shadowRoot.querySelector('supervised-user-error')";
}  // namespace

// Base class for AddSupervision tests.
class AddSupervisionBrowserTest : public InProcessBrowserTest {
 public:
  AddSupervisionBrowserTest() = default;

  AddSupervisionBrowserTest(const AddSupervisionBrowserTest&) = delete;
  AddSupervisionBrowserTest& operator=(const AddSupervisionBrowserTest&) =
      delete;

  ~AddSupervisionBrowserTest() override = default;

  void SetUpOnMainThread() override {
    embedded_test_server()->StartAcceptingConnections();
    // TODO(danan):  See if this is possible to do this instead using
    // FakeGaia.IssueOAuthToken().
    identity_test_env_ = std::make_unique<signin::IdentityTestEnvironment>();
    identity_test_env_->MakePrimaryAccountAvailable(
        "example@gmail.com", signin::ConsentLevel::kSync);
    // This makes the identity manager return the string "access_token" for the
    // access token.
    identity_test_env_->SetAutomaticIssueOfAccessTokens(true);
    AddSupervisionUI::SetUpForTest(identity_test_env_->identity_manager());

    // Set start_time_ so that the DCHECK(!start_time_.is_null()) in
    // AddSupervisionMetricsRecorder::RecordUserTime() doesn't throw.
    AddSupervisionMetricsRecorder::GetInstance()
        ->RecordAddSupervisionEnrollment(
            AddSupervisionMetricsRecorder::EnrollmentState::kInitiated);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    command_line->AppendSwitchASCII(
        "add-supervision-url",
        embedded_test_server()->GetURL("/supervised_user/simple.html").spec());
  }

  AddSupervisionUI* GetAddSupervisionUI() {
    return static_cast<AddSupervisionUI*>(
        contents()->GetWebUI()->GetController());
  }

  content::WebContents* contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  GURL settings_webui_url() { return GURL(chrome::kChromeUISettingsURL); }

  GURL add_supervision_webui_url() {
    return GURL(chrome::kChromeUIAddSupervisionURL);
  }

  bool IsScreenActive(const std::string& element_selector) {
    std::string is_active = element_selector + ".classList.contains('active');";
    return content::EvalJs(contents(), is_active).ExtractBool();
  }

 private:
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;
};

IN_PROC_BROWSER_TEST_F(AddSupervisionBrowserTest, URLParameters) {
  // Open the Add Supervision URL.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), add_supervision_webui_url()));
  EXPECT_TRUE(content::WaitForLoadStop(contents()));

  // Get the URL from the embedded webview.
  std::string webview_url =
      content::EvalJs(
          contents(),
          base::StrCat(
              {kGetAddSupervisionUIElementJS,
               ".shadowRoot.querySelector('#webview').getAttribute('src')",
               ";"}))
          .ExtractString();

  GURL webview_gurl(webview_url);
  ASSERT_TRUE(webview_gurl.has_query());

  // Split the query string into a map of keys to values.
  std::string query_str = webview_gurl.query();
  url::Component query(0, query_str.length());
  url::Component key;
  url::Component value;
  std::map<std::string, std::string> query_parts;
  while (url::ExtractQueryKeyValue(query_str, &query, &key, &value)) {
    query_parts[query_str.substr(key.begin, key.len)] =
        query_str.substr(value.begin, value.len);
  }

  // Validate the query parameters.
  ASSERT_EQ(query_parts.at("flow_type"), "1");
  ASSERT_EQ(query_parts.at("platform_version"),
            base::SysInfo::OperatingSystemVersion());
  ASSERT_EQ(query_parts.at("access_token"), "access_token");
  ASSERT_EQ(query_parts.at("hl"), "en-US");
}

IN_PROC_BROWSER_TEST_F(AddSupervisionBrowserTest, ShowOfflineScreen) {
  // Open the Add Supervision URL.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), add_supervision_webui_url()));
  EXPECT_TRUE(content::WaitForLoadStop(contents()));

  // The online screen should be initially active.
  EXPECT_TRUE(IsScreenActive(std::string(kGetAddSupervisionUIElementJS)));

  EXPECT_FALSE(IsScreenActive(std::string(kGetSupervisedUserOfflineElementJS)));

  // Simulate going offline.
  EXPECT_TRUE(content::ExecJs(
      contents(), "window.dispatchEvent(new CustomEvent('offline'));"));

  // Ensure only the offline screen is active.
  EXPECT_TRUE(IsScreenActive(std::string(kGetSupervisedUserOfflineElementJS)));

  EXPECT_FALSE(IsScreenActive(std::string(kGetAddSupervisionUIElementJS)));

  // Simulate going online.
  EXPECT_TRUE(content::ExecJs(
      contents(), "window.dispatchEvent(new CustomEvent('online'));"));

  // Ensure only the online screen is active.
  EXPECT_TRUE(IsScreenActive(std::string(kGetAddSupervisionUIElementJS)));

  EXPECT_FALSE(IsScreenActive(std::string(kGetSupervisedUserOfflineElementJS)));
}

IN_PROC_BROWSER_TEST_F(AddSupervisionBrowserTest, ShowConfirmSignoutDialog) {
  // Open the Add Supervision URL.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), add_supervision_webui_url()));
  EXPECT_TRUE(content::WaitForLoadStop(contents()));

  // Request that the dialog close before supervision has been enabled.
  ASSERT_TRUE(content::ExecJs(
      contents(), std::string(kGetAddSupervisionUIElementJS) +
                      std::string(".getApiServerForTest().requestClose()")));
  // Confirm that the signout dialog isn't showing
  ASSERT_FALSE(ConfirmSignoutDialog::IsShowing());

  // Simulate supervision being enabled.
  ASSERT_TRUE(content::ExecJs(
      contents(),
      std::string(kGetAddSupervisionUIElementJS) +
          std::string(".getApiServerForTest().notifySupervisionEnabled()")));

  // Request that the dialog is closed again.
  ASSERT_TRUE(content::ExecJs(
      contents(), std::string(kGetAddSupervisionUIElementJS) +
                      std::string(".getApiServerForTest().requestClose()")));

  // Confirm that the dialog is showing.
  ASSERT_TRUE(ConfirmSignoutDialog::IsShowing());
}

IN_PROC_BROWSER_TEST_F(AddSupervisionBrowserTest, UMATest) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  // Should see 0 Add Supervision enrollment metrics at first.
  histogram_tester.ExpectTotalCount("AddSupervisionDialog.Enrollment", 0);

  // Should see 0 user actions at first.
  EXPECT_EQ(user_action_tester.GetActionCount(
                "AddSupervisionDialog_AttemptedSignoutAfterEnrollment"),
            0);

  // Open the Add Supervision URL.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), add_supervision_webui_url()));
  EXPECT_TRUE(content::WaitForLoadStop(contents()));

  // Simulate supervision being enabled.
  ASSERT_TRUE(content::ExecJs(
      contents(),
      std::string(kGetAddSupervisionUIElementJS) +
          std::string(".getApiServerForTest().notifySupervisionEnabled()")));

  // Should see 1 Add Supervision process completed.
  histogram_tester.ExpectUniqueSample(
      "AddSupervisionDialog.Enrollment",
      AddSupervisionMetricsRecorder::EnrollmentState::kCompleted, 1);
  histogram_tester.ExpectTotalCount("AddSupervisionDialog.Enrollment", 1);

  // Should see 1 EnrollmentCompleted action.
  EXPECT_EQ(user_action_tester.GetActionCount(
                "AddSupervisionDialog_EnrollmentCompleted"),
            1);
}

IN_PROC_BROWSER_TEST_F(AddSupervisionBrowserTest, ShowErrorScreen) {
  // Open the Add Supervision URL.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), add_supervision_webui_url()));
  EXPECT_TRUE(content::WaitForLoadStop(contents()));

  // The online screen should be initially active.
  EXPECT_TRUE(IsScreenActive(std::string(kGetAddSupervisionUIElementJS)));

  // Simulate an error event.
  EXPECT_TRUE(content::ExecJs(
      contents(), std::string(kGetAddSupervisionAppElementJS) +
                      std::string(".dispatchEvent(new CustomEvent('show-error',"
                                  "{bubbles: true, composed: true}));")));

  // Ensure that the error screen is active.
  EXPECT_TRUE(IsScreenActive(std::string(kGetSupervisedUserErrorElementJS)));

  EXPECT_FALSE(IsScreenActive(std::string(kGetAddSupervisionUIElementJS)));

  // Simulate an offline event.
  EXPECT_TRUE(content::ExecJs(
      contents(), "window.dispatchEvent(new CustomEvent('offline'));"));

  // Ensure that the error screen remains active.
  EXPECT_TRUE(IsScreenActive(std::string(kGetSupervisedUserErrorElementJS)));

  EXPECT_FALSE(IsScreenActive(std::string(kGetSupervisedUserOfflineElementJS)));
}

}  // namespace ash
