// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_INTEGRATION_BROWSERTEST_H_
#define CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_INTEGRATION_BROWSERTEST_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/test/media_router/media_router_base_browsertest.h"
#include "chrome/test/media_router/media_router_ui_for_test.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"

namespace media_router {

class MediaRouterIntegrationBrowserTest : public MediaRouterBaseBrowserTest {
 public:
  MediaRouterIntegrationBrowserTest();
  ~MediaRouterIntegrationBrowserTest() override;

 protected:
  // InProcessBrowserTest Overrides
  void TearDownOnMainThread() override;
  void SetUpInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;

  // MediaRouterBaseBrowserTest Overrides
  void ParseCommandLine() override;

  // Checks that the request initiated from |web_contents| to start presentation
  // failed with expected |error_name| and |error_message_substring|.
  void CheckStartFailed(content::WebContents* web_contents,
                        const std::string& error_name,
                        const std::string& error_message_substring);

  // Execute javascript and check the return value.
  static void ExecuteJavaScriptAPI(content::WebContents* web_contents,
                            const std::string& script);

  static int ExecuteScriptAndExtractInt(
      const content::ToRenderFrameHost& adapter,
      const std::string& script);

  static std::string ExecuteScriptAndExtractString(
      const content::ToRenderFrameHost& adapter, const std::string& script);

  static bool ExecuteScriptAndExtractBool(
      const content::ToRenderFrameHost& adapter,
      const std::string& script);

  static void ExecuteScript(const content::ToRenderFrameHost& adapter,
                            const std::string& script);

  // Opens "basic_test.html" and asserts that attempting to start a presentation
  // fails with NotFoundError due to no sinks available.
  void StartSessionAndAssertNotFoundError();

  // Opens "basic_test.html," waits for sinks to be available, and starts a
  // presentation.
  content::WebContents* StartSessionWithTestPageAndSink();

  // Opens "basic_test.html," waits for sinks to be available, starts a
  // presentation, and chooses a sink with the name |kTestSinkName|. Also checks
  // that the presentation has successfully started if |should_succeed| is true.
  content::WebContents* StartSessionWithTestPageAndChooseSink();

  // Opens the MR dialog and clicks through the motions of casting a file. Sets
  // up the route provider to succeed or otherwise based on |route_success|.
  // Note: The system dialog portion has to be mocked out as it cannot be
  // simulated.
  void OpenDialogAndCastFile(bool route_success = true);

  // Opens the MR dialog and clicks through the motions of choosing to cast
  // file, file returns an issue. Note: The system dialog portion has to be
  // mocked out as it cannot be simulated.
  void OpenDialogAndCastFileFails();

  void OpenTestPage(base::FilePath::StringPieceType file);
  void OpenTestPageInNewTab(base::FilePath::StringPieceType file);
  virtual GURL GetTestPageUrl(const base::FilePath& full_path);

  void SetTestData(base::FilePath::StringPieceType test_data_file);

  bool IsRouteCreatedOnUI();

  bool IsRouteClosedOnUI();

  // Returns true if there is an issue showing in the UI.
  bool IsUIShowingIssue();

  // Returns the route ID for the specific sink.
  std::string GetRouteId(const std::string& sink_id);

  // Checks that the presentation started for |web_contents| has connected and
  // is the default presentation.
  void CheckSessionValidity(content::WebContents* web_contents);

  // Returns the active WebContents for the current window.
  content::WebContents* GetActiveWebContents();

  // Runs a basic test in which a presentation is created through the
  // MediaRouter dialog, then terminated.
  void RunBasicTest();

  // Runs a test in which we start a presentation and send a message.
  void RunSendMessageTest(const std::string& message);

  // Runs a test in which we start a presentation, close it and send a message.
  void RunFailToSendMessageTest();

  // Runs a test in which we start a presentation and reconnect to it from
  // another tab.
  void RunReconnectSessionTest();

  // Runs a test in which we start a presentation and reconnect to it from the
  // same tab.
  void RunReconnectSessionSameTabTest();

  // Sets whether media router is enabled.
  void SetEnableMediaRouter(bool enable);

  // Test API for manipulating the UI.
  MediaRouterUiForTest* test_ui_ = nullptr;

  // Enabled features.
  base::test::ScopedFeatureList scoped_feature_list_;

  // Name of the test receiver to use.
  std::string receiver_;

 private:
  // Get the full path of the resource file.
  // |relative_path|: The relative path to
  //                  <chromium src>/out/<build config>/media_router/
  //                  browser_test_resources/
  base::FilePath GetResourceFile(
      base::FilePath::StringPieceType relative_path) const;

  std::unique_ptr<content::TestNavigationObserver> test_navigation_observer_;
  policy::MockConfigurationPolicyProvider provider_;
};

class MediaRouterIntegrationIncognitoBrowserTest
    : public MediaRouterIntegrationBrowserTest {
 protected:
  void InstallAndEnableMRExtension() override;
  void UninstallMRExtension() override;
  Browser* browser() override;

 private:
  Browser* incognito_browser_ = nullptr;
  std::string incognito_extension_id_;
};

}  // namespace media_router

#endif  // CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_INTEGRATION_BROWSERTEST_H_
