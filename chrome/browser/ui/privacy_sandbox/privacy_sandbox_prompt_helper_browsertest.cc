// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/privacy_sandbox/mock_privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search/ntp_test_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/webui/help_app_ui/url_constants.h"
#endif

namespace {

const char kPrivacySandboxDialogDisplayHostHistogram[] =
    "Settings.PrivacySandbox.DialogDisplayHost";

std::unique_ptr<KeyedService> CreateTestSyncService(content::BrowserContext*) {
  return std::make_unique<syncer::TestSyncService>();
}

std::unique_ptr<KeyedService> CreateMockPrivacySandboxService(
    content::BrowserContext*) {
  return std::make_unique<testing::NiceMock<MockPrivacySandboxService>>();
}

}  // namespace

class PrivacySandboxPromptHelperTest : public InProcessBrowserTest {
 public:
  PrivacySandboxPromptHelperTest()
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpInProcessBrowserTestFixture() override {
    PrivacySandboxService::SetPromptDisabledForTests(false);
    host_resolver()->AddRule("*", "127.0.0.1");
    https_test_server()->AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));
    ASSERT_TRUE(https_test_server()->Start());
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &PrivacySandboxPromptHelperTest::SetupTestFactories,
                base::Unretained(this)));
  }

  void SetupTestFactories(content::BrowserContext* context) {
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&CreateTestSyncService));
    auto* mock_privacy_sandbox_service =
        static_cast<MockPrivacySandboxService*>(
            PrivacySandboxServiceFactory::GetInstance()
                ->SetTestingFactoryAndUse(
                    context,
                    base::BindRepeating(&CreateMockPrivacySandboxService)));

    ON_CALL(*mock_privacy_sandbox_service, GetRequiredPromptType())
        .WillByDefault(testing::Return(TestPromptType()));
    ON_CALL(*mock_privacy_sandbox_service, IsPromptOpenForBrowser(testing::_))
        .WillByDefault(testing::Return(false));
  }

  virtual PrivacySandboxService::PromptType TestPromptType() {
    return PrivacySandboxService::PromptType::kNone;
  }

  syncer::TestSyncService* test_sync_service() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(browser()->profile()));
  }
  MockPrivacySandboxService* mock_privacy_sandbox_service() {
    return static_cast<MockPrivacySandboxService*>(
        PrivacySandboxServiceFactory::GetForProfile(browser()->profile()));
  }
  net::EmbeddedTestServer* https_test_server() { return &https_test_server_; }

 private:
  base::CallbackListSubscription create_services_subscription_;
  net::EmbeddedTestServer https_test_server_;
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxPromptHelperTest, NoPromptRequired) {
  // Check when no prompt is required, it is not shown.
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              PromptOpenedForBrowser(browser()))
      .Times(0);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUINewTabPageURL)));
  base::RunLoop().RunUntilIdle();
}

class PrivacySandboxPromptHelperTestWithParam
    : public PrivacySandboxPromptHelperTest,
      public testing::WithParamInterface<bool> {
  PrivacySandboxService::PromptType TestPromptType() override {
    // Setup consent / notice based on testing parameter. Helper behavior should
    // be identical regardless of which type of prompt is required.
    return GetParam() ? PrivacySandboxService::PromptType::kConsent
                      : PrivacySandboxService::PromptType::kNotice;
  }
};

IN_PROC_BROWSER_TEST_P(PrivacySandboxPromptHelperTestWithParam,
                       PromptOpensOnNtp) {
  // Check when a navigation to the Chrome controlled NTP occurs, which is a
  // suitable location, a prompt is shown.
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              PromptOpenedForBrowser(browser()))
      .Times(1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUINewTabPageURL)));
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxDialogDisplayHostHistogram,
      static_cast<base::HistogramBase::Sample>(base::Hash("new-tab-page")), 1);
}

IN_PROC_BROWSER_TEST_P(PrivacySandboxPromptHelperTestWithParam,
                       PromptOpensAboutBlank) {
  // Check when a navigation to about:blank occurs, which is a suitable
  // location, a prompt is shown.
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              PromptOpenedForBrowser(browser()))
      .Times(1);
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              IsPromptOpenForBrowser(browser()))
      .Times(1)
      .WillOnce(testing::Return(false));

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxDialogDisplayHostHistogram,
      static_cast<base::HistogramBase::Sample>(base::Hash("about:blank")), 1);
}

IN_PROC_BROWSER_TEST_P(PrivacySandboxPromptHelperTestWithParam,
                       PromptOpensOnSettings) {
  // Check when a navigation to the Chrome settings occurs, which is a
  // suitable location, a prompt is shown.
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              PromptOpenedForBrowser(browser()))
      .Times(1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUISettingsURL)));
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxDialogDisplayHostHistogram,
      static_cast<base::HistogramBase::Sample>(base::Hash("settings")), 1);
}

IN_PROC_BROWSER_TEST_P(PrivacySandboxPromptHelperTestWithParam,
                       PromptOpensOnHistory) {
  // Check when a navigation to the Chrome history occurs, which is a
  // suitable location, a prompt is shown.
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              PromptOpenedForBrowser(browser()))
      .Times(1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIHistoryURL)));
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxDialogDisplayHostHistogram,
      static_cast<base::HistogramBase::Sample>(base::Hash("history")), 1);
}

IN_PROC_BROWSER_TEST_P(PrivacySandboxPromptHelperTestWithParam,
                       NoPromptNonDefaultNtp) {
  // Check that navigations to the generic chrome://newtab, when a non default
  // NTP is used, do not show a prompt.
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              PromptOpenedForBrowser(browser()))
      .Times(0);

  GURL ntp_url = https_test_server()->GetURL("/title1.html");
  ntp_test_utils::SetUserSelectedDefaultSearchProvider(
      browser()->profile(), https_test_server()->base_url().spec(),
      ntp_url.spec());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(kPrivacySandboxDialogDisplayHostHistogram,
                                    0);
}

IN_PROC_BROWSER_TEST_P(PrivacySandboxPromptHelperTestWithParam, NoPromptSync) {
  // Check when sync setup is in progress, that no prompt is shown.
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              PromptOpenedForBrowser(browser()))
      .Times(0);
  test_sync_service()->SetSetupInProgress(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUINewTabPageURL)));
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(kPrivacySandboxDialogDisplayHostHistogram,
                                    0);
}

IN_PROC_BROWSER_TEST_P(PrivacySandboxPromptHelperTestWithParam, UnsuitableUrl) {
  // Check that no prompt is shown for navigations to unsuitable URLs.
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              PromptOpenedForBrowser(browser()))
      .Times(0);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIWelcomeURL)));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server()->GetURL("a.test", "/title1.html")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GURL(chrome::kChromeUISettingsURL).Resolve(chrome::kAutofillSubPage)));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(ash::kChromeUIHelpAppURL)));
#endif
#if BUILDFLAG(IS_CHROMEOS)
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIOSSettingsURL)));
#endif
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(kPrivacySandboxDialogDisplayHostHistogram,
                                    0);
}

IN_PROC_BROWSER_TEST_P(PrivacySandboxPromptHelperTestWithParam,
                       SinglePromptPerBrowser) {
  // Check that only a single prompt is opened per browser window at a time.
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              PromptOpenedForBrowser(browser()))
      .Times(1);
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              IsPromptOpenForBrowser(browser()))
      .WillOnce(testing::Return(false))
      .WillRepeatedly(testing::Return(true));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUINewTabPageURL)));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUINewTabPageURL)));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUINewTabPageURL)));
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxDialogDisplayHostHistogram,
      static_cast<base::HistogramBase::Sample>(base::Hash("new-tab-page")), 1);
}

IN_PROC_BROWSER_TEST_P(PrivacySandboxPromptHelperTestWithParam,
                       MultipleBrowserWindows) {
  // Check that if multiple browser windows are opened, and navigated to
  // appropriate tabs, two prompts are opened.
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              PromptOpenedForBrowser(testing::_))
      .Times(2);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabPageURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  histogram_tester.ExpectBucketCount(
      kPrivacySandboxDialogDisplayHostHistogram,
      static_cast<base::HistogramBase::Sample>(base::Hash("new-tab-page")), 1);
  histogram_tester.ExpectBucketCount(
      kPrivacySandboxDialogDisplayHostHistogram,
      static_cast<base::HistogramBase::Sample>(base::Hash("about:blank")), 1);
}

INSTANTIATE_TEST_SUITE_P(PrivacySandboxPromptHelperTestWithParamInstance,
                         PrivacySandboxPromptHelperTestWithParam,
                         testing::Bool());
