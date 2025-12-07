// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/history_sync_optin/history_sync_optin_handler.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin/history_sync_optin.mojom.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using history_sync_optin::mojom::ScreenMode;
using testing::_;
using testing::Return;

class MockHistorySyncOptinPage : public history_sync_optin::mojom::Page {
 public:
  MockHistorySyncOptinPage() = default;
  ~MockHistorySyncOptinPage() override = default;

  mojo::PendingRemote<history_sync_optin::mojom::Page> BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void, SendScreenMode, (ScreenMode screen_mode), (override));
  MOCK_METHOD(void,
              SendAccountInfo,
              (history_sync_optin::mojom::AccountInfoPtr account_info),
              (override));

  mojo::Receiver<history_sync_optin::mojom::Page> receiver_{this};
};

class HistorySyncOptinHandlerTest : public testing::TestWithParam<bool> {
 public:
  HistorySyncOptinHandlerTest() {
    TestingProfile::Builder builder;
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(builder);
    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
  }

  void SetUp() override {
    handler_ = std::make_unique<HistorySyncOptinHandler>(
        handler_remote_.BindNewPipeAndPassReceiver(), page_.BindAndGetRemote(),
        /*browser=*/nullptr, profile(), /*should_close_modal_dialog=*/true,
        HistorySyncOptinHelper::FlowCompletedCallback(base::DoNothing()));
  }

  void TearDown() override { handler_.reset(); }

  bool IsUnrestricted() { return GetParam(); }

  // Returns the expected mode iff capabilities are set.
  ScreenMode ExpectedScreenMode() {
    return IsUnrestricted() ? ScreenMode::kUnrestricted
                            : ScreenMode::kRestricted;
  }

  TestingProfile* profile() { return profile_.get(); }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

  syncer::TestSyncService* test_sync_service() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(profile()));
  }

  AccountInfo SignInAndSetUpSyncService() {
    AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
        "test@gmail.com", signin::ConsentLevel::kSignin);

    account_info = AccountInfo::Builder(account_info)
                       .SetFullName("fullname")
                       .SetGivenName("givenname")
                       .SetHostedDomain("gmail.com")
                       .SetAvatarUrl("https://example.com")
                       .Build();
    CHECK(account_info.IsValid());
    identity_test_env()->UpdateAccountInfoForAccount(account_info);

    SyncServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return std::make_unique<syncer::TestSyncService>();
        }));
    CHECK(test_sync_service());

    return account_info;
  }

  void DisableAllSyncedDataTypes() {
    test_sync_service()->GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false, syncer::UserSelectableTypeSet());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  testing::NiceMock<MockHistorySyncOptinPage> page_;
  mojo::Remote<history_sync_optin::mojom::PageHandler> handler_remote_;
  std::unique_ptr<HistorySyncOptinHandler> handler_;
  base::HistogramTester histogram_tester_;
};

// Tests that the handler sends the AccountInfo and ScreenMode when requested.
TEST_P(HistorySyncOptinHandlerTest,
       RequestAccountInfoWithImmediatelyAvailableCapabilities) {
  AccountInfo account_info = SignInAndSetUpSyncService();
  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
      IsUnrestricted());
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // The AccountInfo and the ScreenMode are sent only once.
  EXPECT_CALL(page_, SendAccountInfo(_)).Times(1);
  EXPECT_CALL(page_, SendScreenMode(ExpectedScreenMode())).Times(1);
  handler_->RequestAccountInfo();

  // Simulate an AccountInfo update.
  account_info.full_name = "new_fullname";
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // Attempt to request the AccountInfo, which the handler will ignore.
  handler_->RequestAccountInfo();

  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectUniqueSample(
      "Signin.SyncButtons.Shown",
      IsUnrestricted()
          ? signin_metrics::SyncButtonsType::kSyncNotEqualWeighted
          : signin_metrics::SyncButtonsType::kSyncEqualWeightedFromCapability,
      1);
  histogram_tester_.ExpectTotalCount(
      "Signin.AccountCapabilities.UserVisibleLatency", 1);
  histogram_tester_.ExpectTotalCount("Signin.AccountCapabilities.FetchLatency",
                                     0);
  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountCapabilities.ImmediatelyAvailable", true, 1);
}

// Tests that the handler sends the ScreenMode when capabilities become
// available.
TEST_P(HistorySyncOptinHandlerTest, RequestAccountInfoWithCapabilitiesUpdate) {
  AccountInfo account_info = SignInAndSetUpSyncService();
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // The AccountInfo update, which contains Avatar info, is sent only once.
  EXPECT_CALL(page_, SendAccountInfo(_)).Times(1);
  handler_->RequestAccountInfo();

  // The ScreenMode is sent only once.
  EXPECT_CALL(page_, SendScreenMode(ExpectedScreenMode())).Times(1);

  // Simulate an AccountInfo update without ScreenMode information.
  account_info.full_name = "new_fullname";
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // Simulate an AccountInfo update with capabilities. The handler will send
  // a ScreenMode update, but not a second AccountInfo update.
  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
      IsUnrestricted());
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // Simulate another AccountInfo update, which the handler will not receive.
  account_info.given_name = "new_givenname";
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectUniqueSample(
      "Signin.SyncButtons.Shown",
      IsUnrestricted()
          ? signin_metrics::SyncButtonsType::kSyncNotEqualWeighted
          : signin_metrics::SyncButtonsType::kSyncEqualWeightedFromCapability,
      1);
  histogram_tester_.ExpectTotalCount(
      "Signin.AccountCapabilities.UserVisibleLatency", 1);
  histogram_tester_.ExpectTotalCount("Signin.AccountCapabilities.FetchLatency",
                                     1);
  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountCapabilities.ImmediatelyAvailable", false, 1);
}

// Tests that user settings are updated on accepting the History Sync dialog.
TEST_P(HistorySyncOptinHandlerTest, OnScreenModeAccepted) {
  AccountInfo account_info = SignInAndSetUpSyncService();
  DisableAllSyncedDataTypes();
  EXPECT_FALSE(test_sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));

  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
      IsUnrestricted());
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  handler_->RequestAccountInfo();
  handler_->Accept();
  EXPECT_TRUE(test_sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));
}

// Tests that user settings are updated on rejecting the History Sync dialog.
TEST_P(HistorySyncOptinHandlerTest, OnScreenModeRejected) {
  AccountInfo account_info = SignInAndSetUpSyncService();
  DisableAllSyncedDataTypes();
  EXPECT_FALSE(test_sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));

  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
      IsUnrestricted());
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  handler_->RequestAccountInfo();
  handler_->Reject();
  EXPECT_FALSE(test_sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));
}

// Tests that the handler send ScreenMode::kDeadlined if account capabilities
// are not available before the deadline.
TEST_P(HistorySyncOptinHandlerTest, OnScreenModeTimeout) {
  AccountInfo account_info = SignInAndSetUpSyncService();
  DisableAllSyncedDataTypes();
  EXPECT_FALSE(test_sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));

  handler_->RequestAccountInfo();
  EXPECT_CALL(page_, SendScreenMode(ScreenMode::kDeadlined));
  task_environment_.FastForwardBy(base::Seconds(1));

  handler_->Accept();
  EXPECT_TRUE(test_sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));

  histogram_tester_.ExpectUniqueSample(
      "Signin.SyncButtons.Shown",
      signin_metrics::SyncButtonsType::kSyncEqualWeightedFromDeadline, 1);
  histogram_tester_.ExpectTotalCount(
      "Signin.AccountCapabilities.UserVisibleLatency", 1);
  histogram_tester_.ExpectTotalCount("Signin.AccountCapabilities.FetchLatency",
                                     1);
  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountCapabilities.ImmediatelyAvailable", false, 1);
}

// Tests that the dialog does not crash if a button is pressed more than once.
// Regression test for crbug.com/449140137.
TEST_P(HistorySyncOptinHandlerTest, DoubleClickingDoesNotCrash) {
  AccountInfo account_info = SignInAndSetUpSyncService();
  DisableAllSyncedDataTypes();
  handler_->Accept();
  handler_->Reject();
}

// This boolean parameter controls the value of the account capability
// `can_show_history_sync_opt_ins_without_minor_mode_restrictions`.
INSTANTIATE_TEST_SUITE_P(
    All,
    HistorySyncOptinHandlerTest,
    ::testing::Bool(),  // `true` for unrestricted, `false` for restricted.
    [](const testing::TestParamInfo<HistorySyncOptinHandlerTest::ParamType>&
           info) -> std::string {
      return info.param ? "Unrestricted" : "Restricted";
    });

}  // namespace
