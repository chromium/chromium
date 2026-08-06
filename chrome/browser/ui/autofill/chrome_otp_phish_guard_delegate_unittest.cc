// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/chrome_otp_phish_guard_delegate.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_debug_features.h"
#include "components/safe_browsing/core/browser/db/test_database_manager.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "components/tabs/public/tab_interface.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace autofill {

namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SaveArg;

class MockSafeBrowsingDatabaseManager
    : public safe_browsing::TestSafeBrowsingDatabaseManager {
 public:
  MockSafeBrowsingDatabaseManager()
      : safe_browsing::TestSafeBrowsingDatabaseManager(
            base::SequencedTaskRunner::GetCurrentDefault()) {}

  MockSafeBrowsingDatabaseManager(const MockSafeBrowsingDatabaseManager&) =
      delete;
  MockSafeBrowsingDatabaseManager& operator=(
      const MockSafeBrowsingDatabaseManager&) = delete;

  MOCK_METHOD(void,
              CancelCheck,
              (safe_browsing::SafeBrowsingDatabaseManager::Client*),
              (override));

  MOCK_METHOD(bool,
              CheckBrowseUrl,
              (const GURL&,
               const safe_browsing::SBThreatTypeSet&,
               safe_browsing::SafeBrowsingDatabaseManager::Client*,
               safe_browsing::CheckBrowseUrlType),
              (override));

 protected:
  ~MockSafeBrowsingDatabaseManager() override = default;
};

}  // namespace

class ChromeOtpPhishGuardDelegateTest : public ChromeRenderViewHostTestHarness {
 public:
  ChromeOtpPhishGuardDelegateTest()
      : main_frame_url_("https://main-frame.example.com"),
        frame_to_fill_url_("https://iframe.example.com") {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

#if !BUILDFLAG(IS_ANDROID)
    actor::ActorKeyedServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return std::make_unique<actor::ActorKeyedServiceFake>(
              Profile::FromBrowserContext(context));
        }));
#endif

    browser_process_ = TestingBrowserProcess::GetGlobal();

    database_manager_ = base::MakeRefCounted<MockSafeBrowsingDatabaseManager>();
    safe_browsing_factory_ =
        std::make_unique<safe_browsing::TestSafeBrowsingServiceFactory>();
    safe_browsing_factory_->SetTestDatabaseManager(database_manager_.get());

    browser_process_->SetSafeBrowsingService(
        safe_browsing_factory_->CreateSafeBrowsingService());
    browser_process_->safe_browsing_service()->Initialize();

    // Enable SafeBrowsing by default.
    safe_browsing::SetSafeBrowsingState(
        profile()->GetPrefs(),
        safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);
  }

  void TearDown() override {
    browser_process_->safe_browsing_service()->ShutDown();
    browser_process_->SetSafeBrowsingService(nullptr);
    database_manager_ = nullptr;

    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;
  raw_ptr<TestingBrowserProcess> browser_process_ = nullptr;
  scoped_refptr<MockSafeBrowsingDatabaseManager> database_manager_;
  std::unique_ptr<safe_browsing::TestSafeBrowsingServiceFactory>
      safe_browsing_factory_;

  GURL main_frame_url_;
  GURL frame_to_fill_url_;
};

// Test that when SafeBrowsing is disabled, PhishGuard check reports false
// (safe/not phishing) instantly.
TEST_F(ChromeOtpPhishGuardDelegateTest, SafeBrowsingDisabled) {
  safe_browsing::SetSafeBrowsingState(
      profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING);

  // We should not make database check calls if SafeBrowsing is disabled.
  EXPECT_CALL(*database_manager_, CheckBrowseUrl).Times(0);

  base::test::TestFuture<bool> future;
  auto delegate = std::make_unique<ChromeOtpPhishGuardDelegate>(web_contents());
  delegate->StartOtpPhishGuardCheck(main_frame_url_, frame_to_fill_url_,
                                    future.GetCallback());
  EXPECT_FALSE(future.Get());
}

// `ActorKeyedService` and actor mode tab capabilities are only supported on
// Desktop.
#if !BUILDFLAG(IS_ANDROID)
// Test that when SafeBrowsing is disabled and an actor task is ongoing,
// PhishGuard check reports true (unsafe/malicious) to fail closed.
TEST_F(ChromeOtpPhishGuardDelegateTest, SafeBrowsingDisabled_ActorTaskOngoing) {
  actor::TestTabState tab_state(web_contents());
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &tab_state.tab);

  auto* actor_service = static_cast<actor::ActorKeyedServiceFake*>(
      actor::ActorKeyedService::Get(profile()));
  actor::TaskId task_id = actor_service->CreateTaskForTesting();

  auto* task = actor_service->GetTask(task_id);
  task->SetState(actor::ActorTask::State::kActing);
  task->AddTab(tab_state.tab.GetHandle(), /*stop_task_on_detach=*/false,
               base::DoNothing());

  ChromeAutofillClient::CreateForWebContents(web_contents());
  ChromePasswordManagerClient::CreateForWebContents(web_contents());

  safe_browsing::SetSafeBrowsingState(
      profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING);

  // We should not make database check calls if SafeBrowsing is disabled.
  EXPECT_CALL(*database_manager_, CheckBrowseUrl).Times(0);

  base::test::TestFuture<bool> future;
  auto delegate = std::make_unique<ChromeOtpPhishGuardDelegate>(web_contents());
  delegate->StartOtpPhishGuardCheck(main_frame_url_, frame_to_fill_url_,
                                    future.GetCallback());
  EXPECT_TRUE(future.Get());
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Test that when both URLs are safe, the delegate callback is run with false
// (safe/not phishing).
TEST_F(ChromeOtpPhishGuardDelegateTest, BothUrlsSafe) {
  EXPECT_CALL(*database_manager_, CheckBrowseUrl(main_frame_url_, _, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*database_manager_, CheckBrowseUrl(frame_to_fill_url_, _, _, _))
      .WillOnce(Return(true));

  base::test::TestFuture<bool> future;
  auto delegate = std::make_unique<ChromeOtpPhishGuardDelegate>(web_contents());
  delegate->StartOtpPhishGuardCheck(main_frame_url_, frame_to_fill_url_,
                                    future.GetCallback());
  EXPECT_FALSE(future.Get());
}

// Test that when the iframe URL is unsafe, the check stops and reports true
// (unsafe/is phishing).
TEST_F(ChromeOtpPhishGuardDelegateTest, IframeUnsafe) {
  // First URL (main frame) is safe.
  EXPECT_CALL(*database_manager_, CheckBrowseUrl(main_frame_url_, _, _, _))
      .WillOnce(Return(true));

  // Second URL (iframe to fill) is unsafe asynchronously.
  base::test::TestFuture<safe_browsing::SafeBrowsingDatabaseManager::Client*>
      sb_client_future;
  EXPECT_CALL(*database_manager_, CheckBrowseUrl(frame_to_fill_url_, _, _, _))
      .WillOnce([&](const GURL&, const safe_browsing::SBThreatTypeSet&,
                    safe_browsing::SafeBrowsingDatabaseManager::Client* client,
                    safe_browsing::CheckBrowseUrlType) {
        sb_client_future.SetValue(client);
        return false;
      });

  base::test::TestFuture<bool> future;
  auto delegate = std::make_unique<ChromeOtpPhishGuardDelegate>(web_contents());
  delegate->StartOtpPhishGuardCheck(main_frame_url_, frame_to_fill_url_,
                                    future.GetCallback());

  safe_browsing::SafeBrowsingDatabaseManager::Client* sb_client =
      sb_client_future.Get();
  ASSERT_TRUE(sb_client);
  // Report phishing for target iframe URL.
  sb_client->OnCheckBrowseUrlResult(
      frame_to_fill_url_,
      safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
  EXPECT_TRUE(future.Get());
}

}  // namespace autofill
