// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search/ntp_test_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sync/driver/test_sync_service.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

const char kPrivacySandboxDialogDisplayHostHistogram[] =
    "Settings.PrivacySandbox.DialogDisplayHost";

class MockPrivacySandboxService : public PrivacySandboxService {
 public:
  MOCK_METHOD(PrivacySandboxService::DialogType,
              GetRequiredDialogType,
              (),
              (override));
  MOCK_METHOD(void, DialogOpenedForBrowser, (Browser*), (override));
  MOCK_METHOD(bool, IsDialogOpenForBrowser, (Browser*), (override));
  MOCK_METHOD(void,
              DialogActionOccurred,
              (PrivacySandboxService::DialogAction),
              (override));
};

std::unique_ptr<KeyedService> CreateTestSyncService(content::BrowserContext*) {
  return std::make_unique<syncer::TestSyncService>();
}

std::unique_ptr<KeyedService> CreateMockPrivacySandboxService(
    content::BrowserContext*) {
  return std::make_unique<testing::NiceMock<MockPrivacySandboxService>>();
}

}  // namespace

class PrivacySandboxDialogHelperTest : public InProcessBrowserTest {
 public:
  PrivacySandboxDialogHelperTest()
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpInProcessBrowserTestFixture() override {
    PrivacySandboxService::SetDialogDisabledForTests(false);
    host_resolver()->AddRule("*", "127.0.0.1");
    https_test_server()->AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));
    ASSERT_TRUE(https_test_server()->Start());
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &PrivacySandboxDialogHelperTest::SetupTestFactories,
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

    ON_CALL(*mock_privacy_sandbox_service, GetRequiredDialogType())
        .WillByDefault(testing::Return(TestDialogType()));
    ON_CALL(*mock_privacy_sandbox_service, IsDialogOpenForBrowser(testing::_))
        .WillByDefault(testing::Return(false));
  }

  virtual PrivacySandboxService::DialogType TestDialogType() {
    return PrivacySandboxService::DialogType::kNone;
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

IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogHelperTest, NoDialogRequired) {
  // Check when no dialog is required, it is not shown.
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              DialogOpenedForBrowser(browser()))
      .Times(0);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUINewTabPageURL)));
  base::RunLoop().RunUntilIdle();
}

class PrivacySandboxDialogHelperTestWithParam
    : public PrivacySandboxDialogHelperTest,
      public testing::WithParamInterface<bool> {
  PrivacySandboxService::DialogType TestDialogType() override {
    // Setup consent / notice based on testing parameter. Helper behavior should
    // be identical regardless of which type of dialog is required.
    return GetParam() ? PrivacySandboxService::DialogType::kConsent
                      : PrivacySandboxService::DialogType::kNotice;
  }
};

IN_PROC_BROWSER_TEST_P(PrivacySandboxDialogHelperTestWithParam,
                       DialogOpensOnNtp) {
  // Check when a navigation to the Chrome controlled NTP occurs, which is a
  // suitable location, a dialog is shown.
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              DialogOpenedForBrowser(browser()))
      .Times(1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUINewTabPageURL)));
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxDialogDisplayHostHistogram,
      static_cast<base::HistogramBase::Sample>(base::Hash("new-tab-page")), 1);
}

IN_PROC_BROWSER_TEST_P(PrivacySandboxDialogHelperTestWithParam,
                       DialogOpensAboutBlank) {
  // Check when a navigation to about:blank occurs, which is a suitable
  // location, a dialog is shown.
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              DialogOpenedForBrowser(browser()))
      .Times(1);
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              IsDialogOpenForBrowser(browser()))
      .Times(1)
      .WillOnce(testing::Return(false));

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      kPrivacySandboxDialogDisplayHostHistogram,
      static_cast<base::HistogramBase::Sample>(base::Hash("about:blank")), 1);
}

IN_PROC_BROWSER_TEST_P(PrivacySandboxDialogHelperTestWithParam,
                       NoDialogNonDefaultNtp) {
  // Check that navigations to the generic chrome://newtab, when a non default
  // NTP is used, do not show a dialog.
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              DialogOpenedForBrowser(browser()))
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

IN_PROC_BROWSER_TEST_P(PrivacySandboxDialogHelperTestWithParam, NoDialogSync) {
  // Check when sync setup is in progress, that no dialog is shown.
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              DialogOpenedForBrowser(browser()))
      .Times(0);
  test_sync_service()->SetSetupInProgress(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUINewTabPageURL)));
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(kPrivacySandboxDialogDisplayHostHistogram,
                                    0);
}

IN_PROC_BROWSER_TEST_P(PrivacySandboxDialogHelperTestWithParam, UnsuitableUrl) {
  // Check that no dialog is shown for navigations to unsuitable URLs.
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              DialogOpenedForBrowser(browser()))
      .Times(0);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIWelcomeURL)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server()->GetURL("a.test", "/title1.html")));
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(kPrivacySandboxDialogDisplayHostHistogram,
                                    0);
}

IN_PROC_BROWSER_TEST_P(PrivacySandboxDialogHelperTestWithParam,
                       SingleDialogPerBrowser) {
  // Check that only a single dialog is opened per browser window at a time.
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              DialogOpenedForBrowser(browser()))
      .Times(1);
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              IsDialogOpenForBrowser(browser()))
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

IN_PROC_BROWSER_TEST_P(PrivacySandboxDialogHelperTestWithParam,
                       MultipleBrowserWindows) {
  // Check that if multiple browser windows are opened, and navigated to
  // appropriate tabs, two dialogs are opened.
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              DialogOpenedForBrowser(testing::_))
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

INSTANTIATE_TEST_SUITE_P(PrivacySandboxDialogHelperTestWithParamInstance,
                         PrivacySandboxDialogHelperTestWithParam,
                         testing::Bool());
