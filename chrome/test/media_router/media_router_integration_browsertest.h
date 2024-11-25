// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_INTEGRATION_BROWSERTEST_H_
#define CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_INTEGRATION_BROWSERTEST_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/media/router/providers/test/test_media_route_provider.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/media_router/media_router_ui_for_test_base.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"

namespace media_router {

enum class UiForBrowserTest {
  // The dedicated Cast dialog
  kCast,
  // The Global Media Controls UI
  kGmc,
};

inline std::string PrintToString(UiForBrowserTest val) {
  switch (val) {
    case UiForBrowserTest::kCast:
      return "Cast";
    case UiForBrowserTest::kGmc:
      return "Gmc";
  }
}

#if BUILDFLAG(IS_CHROMEOS)
// Global media controls aren't supported in lacros.
#define INSTANTIATE_MEDIA_ROUTER_INTEGRATION_BROWER_TEST_SUITE(name) \
  INSTANTIATE_TEST_SUITE_P(/* no prefix */, name,                    \
                           testing::Values(UiForBrowserTest::kCast), \
                           testing::PrintToStringParamName())
#else
#define INSTANTIATE_MEDIA_ROUTER_INTEGRATION_BROWER_TEST_SUITE(name)    \
  INSTANTIATE_TEST_SUITE_P(                                             \
      /* no prefix */, name,                                            \
      testing::Values(UiForBrowserTest::kCast, UiForBrowserTest::kGmc), \
      testing::PrintToStringParamName())
#endif  // BUILDFLAG(IS_CHROMEOS)

// Macro used to skip tests that are only supported with the Cast dialog.
//
// TODO(crbug.com/1229305): Eliminate as many uses of this macro as possible.
#define MEDIA_ROUTER_INTEGRATION_BROWER_TEST_CAST_ONLY() \
  if (GetParam() != UiForBrowserTest::kCast) {           \
    GTEST_SKIP() << "Skipping Cast-only test.";          \
    return;                                              \
  }

class MediaRouterIntegrationBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<UiForBrowserTest> {
 public:
  MediaRouterIntegrationBrowserTest();
  ~MediaRouterIntegrationBrowserTest() override;

  // InProcessBrowserTest Overrides
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUp() override;

 protected:
  void InitTestUi();

  // InProcessBrowserTest Overrides
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;
  void SetUpInProcessBrowserTestFixture() override;

  virtual void ParseCommandLine();

  // Checks that the request initiated from |web_contents| to start
  // presentation failed with expected |error_name| and
  // |error_message_substring|.
  void CheckStartFailed(content::WebContents* web_contents,
                        const std::string& error_name,
                        const std::string& error_message_substring);

  // Execute javascript and check the return value.
  static void ExecuteJavaScriptAPI(content::WebContents* web_contents,
                                   const std::string& script);

  static void ExecuteScript(const content::ToRenderFrameHost& adapter,
                            const std::string& script);

  // Opens "basic_test.html" and asserts that attempting to start a
  // presentation fails with NotFoundError due to no sinks available.
  void StartSessionAndAssertNotFoundError();

  // Opens "basic_test.html," waits for sinks to be available, and
  // starts a presentation.
  content::WebContents* StartSessionWithTestPageAndSink();

  // Opens "basic_test.html," waits for sinks to be available, starts
  // a presentation, and chooses a sink with the name |kTestSinkName|.
  // Also checks that the presentation has successfully started if
  // |should_succeed| is true.
  virtual content::WebContents* StartSessionWithTestPageAndChooseSink();

  void OpenTestPage(base::FilePath::StringPieceType file);
  void OpenTestPageInNewTab(base::FilePath::StringPieceType file);
  virtual GURL GetTestPageUrl(const base::FilePath& full_path);

  void SetTestData(base::FilePath::StringPieceType test_data_file);

  bool IsRouteCreatedOnUI();

  // Returns the route ID for the specific sink.
  std::string GetRouteId(const std::string& sink_id);

  // Checks that the presentation started for |web_contents| has
  // connected and is the default presentation.
  void CheckSessionValidity(content::WebContents* web_contents);

  // Returns the active WebContents for the current window.
  content::WebContents* GetActiveWebContents();

  // Runs a basic test in which a presentation is created through the
  // MediaRouter dialog, then terminated.
  void RunBasicTest();

  // Runs a test in which we start a presentation and send a message.
  void RunSendMessageTest(const std::string& message);

  // Runs a test in which we start a presentation, close it and send a
  // message.
  void RunFailToSendMessageTest();

  // Runs a test in which we start a presentation and reconnect to it
  // from another tab.
  void RunReconnectSessionTest();

  // Runs a test in which we start a presentation and failed to
  // reconnect it from another tab.
  void RunFailedReconnectSessionTest();

  // Runs a test in which we start a presentation and reconnect to it
  // from the same tab.
  void RunReconnectSessionSameTabTest();

  // Sets whether media router is enabled.
  void SetEnableMediaRouter(bool enable);

  // Wait until get the successful callback or timeout.
  // Returns true if the condition is satisfied before the timeout.
  // TODO(leilei): Replace this method with WaitableEvent class.
  bool ConditionalWait(base::TimeDelta timeout,
                       base::TimeDelta interval,
                       const base::RepeatingCallback<bool(void)>& callback);

  // Wait for a specific time.
  void Wait(base::TimeDelta timeout);

  void WaitUntilNoRoutes(content::WebContents* web_contents);

  // Get the full path of the resource file.
  // |relative_path|: The relative path to
  //                  <chromium src>/out/<build config>/media_router/
  //                  browser_test_resources/
  base::FilePath GetResourceFile(
      base::FilePath::StringPieceType relative_path) const;

  // Returns whether actual media route providers (as opposed to
  // TestMediaRouteProvider) should be loaded.
  virtual bool RequiresMediaRouteProviders() const;

  // Test API for manipulating the UI.
  std::unique_ptr<MediaRouterUiForTestBase> test_ui_;

  // Enabled features.
  base::test::ScopedFeatureList scoped_feature_list_;

  // Name of the test receiver to use.
  std::string receiver_;

  std::unique_ptr<TestMediaRouteProvider> test_provider_;

  // Returns the superclass' browser(). Marked virtual so that it can
  // be overridden by MediaRouterIntegrationIncognitoBrowserTest.
  virtual Browser* browser();

 private:
  std::unique_ptr<content::TestNavigationObserver> test_navigation_observer_;
  policy::MockConfigurationPolicyProvider provider_;
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace media_router

#endif  // CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_INTEGRATION_BROWSERTEST_H_
