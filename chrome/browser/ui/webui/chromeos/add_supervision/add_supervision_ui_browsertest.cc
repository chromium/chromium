// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision_metrics_recorder.h"
#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision_ui.h"
#include "chrome/browser/ui/webui/chromeos/add_supervision/confirm_signout_dialog.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/network_connection_change_simulator.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// NOTE: This test is flaky and therefore disabled under MSAN:
// https://crbug.com/1002560
#if !defined(MEMORY_SANITIZER)

namespace {

const char kGetAddSupervisionUIElementJS[] =
    "document.querySelector('add-supervision-ui')";
}

// Base class for AddSupervision tests.
class AddSupervisionBrowserTest : public InProcessBrowserTest {
 public:
  AddSupervisionBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kParentalControlsSettings}, {});
  }
  ~AddSupervisionBrowserTest() override = default;

  void SetUpOnMainThread() override {
    // TODO(danan):  See if this is possible to do this instead using
    // FakeGaia.IssueOAuthToken().
    identity_test_env_ = std::make_unique<signin::IdentityTestEnvironment>();
    identity_test_env_->MakePrimaryAccountAvailable("example@gmail.com");
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

  chromeos::AddSupervisionUI* GetAddSupervisionUI() {
    return static_cast<chromeos::AddSupervisionUI*>(
        contents()->GetWebUI()->GetController());
  }

  content::WebContents* contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  GURL settings_webui_url() { return GURL(chrome::kChromeUISettingsURL); }

  GURL add_supervision_webui_url() {
    return GURL(chrome::kChromeUIAddSupervisionURL);
  }

  bool IsElementVisible(const std::string& element_selector) {
    bool found;
    bool hidden;
    std::string script = std::string("domAutomationController.send(") +
                         element_selector + ".hidden);";
    LOG(ERROR) << "Script: " << script;
    found = content::ExecuteScriptAndExtractBool(contents(), script, &hidden);
    return found && !hidden;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;

  DISALLOW_COPY_AND_ASSIGN(AddSupervisionBrowserTest);
};

// Disabled on ASan and LSAn builds, because it's very flaky. See
// crbug.com/1004237
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER)
#define MAYBE_URLParameters DISABLED_URLParameters
#else
#define MAYBE_URLParameters URLParameters
#endif
IN_PROC_BROWSER_TEST_F(AddSupervisionBrowserTest, MAYBE_URLParameters) {
  // Open the Add Supervision URL.
  ui_test_utils::NavigateToURL(browser(), add_supervision_webui_url());
  content::WaitForLoadStop(contents());

  // Get the URL from the embedded webview.
  std::string webview_url;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      contents(),
      std::string("domAutomationController.send(") +
          std::string(kGetAddSupervisionUIElementJS) +
          ".shadowRoot.querySelector('#webview').getAttribute('src')" +
          std::string(");"),
      &webview_url));

  GURL webview_gurl(webview_url);
  ASSERT_TRUE(webview_gurl.has_query());

  // Split the query string into a map of keys to values.
  std::string query_str = webview_gurl.query();
  url::Component query(0, query_str.length());
  url::Component key;
  url::Component value;
  std::map<std::string, std::string> query_parts;
  while (url::ExtractQueryKeyValue(query_str.c_str(), &query, &key, &value)) {
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
  ui_test_utils::NavigateToURL(browser(), add_supervision_webui_url());
  content::WaitForLoadStop(contents());

  // Webview div should be initially visible.
  ASSERT_TRUE(IsElementVisible(std::string(kGetAddSupervisionUIElementJS) +
                               std::string(".webviewDiv")));

  // Simulate going offline.
  ASSERT_TRUE(content::ExecuteScript(
      contents(), "window.dispatchEvent(new CustomEvent('offline'));"));

  // Ensure the offline content view is shown.
  ASSERT_TRUE(IsElementVisible(std::string(kGetAddSupervisionUIElementJS) +
                               std::string(".offlineContentDiv")));

  // Ensure the online webview content content is hidden.
  ASSERT_FALSE(IsElementVisible(std::string(kGetAddSupervisionUIElementJS) +
                                std::string(".webviewDiv")));

  // Simulate going online.
  ASSERT_TRUE(content::ExecuteScript(
      contents(), "window.dispatchEvent(new CustomEvent('online'));"));

  // Offline div should be hidden.
  ASSERT_FALSE(IsElementVisible(std::string(kGetAddSupervisionUIElementJS) +
                                std::string(".offlineContentDiv")));

  // Webview div should be shown.
  ASSERT_TRUE(IsElementVisible(std::string(kGetAddSupervisionUIElementJS) +
                               std::string(".webviewDiv")));
}

// Disabled on ASan and LSAn builds, because it's very flaky. See
// crbug.com/1004237
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER)
#define MAYBE_ShowConfirmSignoutDialog DISABLED_ShowConfirmSignoutDialog
#else
#define MAYBE_ShowConfirmSignoutDialog ShowConfirmSignoutDialog
#endif
IN_PROC_BROWSER_TEST_F(AddSupervisionBrowserTest,
                       MAYBE_ShowConfirmSignoutDialog) {
  // Open the Add Supervision URL.
  ui_test_utils::NavigateToURL(browser(), add_supervision_webui_url());
  content::WaitForLoadStop(contents());

  // Request that the dialog close before supervision has been enabled.
  ASSERT_TRUE(content::ExecuteScript(
      contents(), std::string(kGetAddSupervisionUIElementJS) +
                      std::string(".server.requestClose()")));
  // Confirm that the signout dialog isn't showing
  ASSERT_FALSE(ConfirmSignoutDialog::IsShowing());

  // Simulate supervision being enabled.
  ASSERT_TRUE(content::ExecuteScript(
      contents(), std::string(kGetAddSupervisionUIElementJS) +
                      std::string(".server.notifySupervisionEnabled()")));

  // Request that the dialog is closed again.
  ASSERT_TRUE(content::ExecuteScript(
      contents(), std::string(kGetAddSupervisionUIElementJS) +
                      std::string(".server.requestClose()")));

  // Confirm that the dialog is showing.
  ASSERT_TRUE(ConfirmSignoutDialog::IsShowing());
}

// Disabled on ASan and LSAn builds, because it's very flaky. See
// crbug.com/1004237
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER)
#define MAYBE_UMATest DISABLED_UMATest
#else
#define MAYBE_UMATest UMATest
#endif
IN_PROC_BROWSER_TEST_F(AddSupervisionBrowserTest, MAYBE_UMATest) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  // Should see 0 Add Supervision enrollment metrics at first.
  histogram_tester.ExpectTotalCount("AddSupervisionDialog.Enrollment", 0);

  // Should see 0 user actions at first.
  EXPECT_EQ(user_action_tester.GetActionCount(
                "AddSupervisionDialog_AttemptedSignoutAfterEnrollment"),
            0);

  // Open the Add Supervision URL.
  ui_test_utils::NavigateToURL(browser(), add_supervision_webui_url());
  content::WaitForLoadStop(contents());

  // Simulate supervision being enabled.
  ASSERT_TRUE(content::ExecuteScript(
      contents(), std::string(kGetAddSupervisionUIElementJS) +
                      std::string(".server.notifySupervisionEnabled()")));

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

#endif  // !defined(MEMORY_SANITIZER)

}  // namespace chromeos
