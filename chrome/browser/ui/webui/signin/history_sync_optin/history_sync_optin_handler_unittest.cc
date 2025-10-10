// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/history_sync_optin/history_sync_optin_handler.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin/history_sync_optin.mojom.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/signin_constants.h"
#include "components/sync/test/test_sync_service.h"
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

  MOCK_METHOD(void,
              SendScreenMode,
              (history_sync_optin::mojom::ScreenMode screen_mode),
              (override));
  MOCK_METHOD(void,
              SendAccountInfo,
              (history_sync_optin::mojom::AccountInfoPtr account_info),
              (override));

  mojo::Receiver<history_sync_optin::mojom::Page> receiver_{this};
};

// TODO(crbug.com/450448198): Consider using a more lightweight test fixture.
class HistorySyncOptinHandlerTest : public BrowserWithTestWindowTest {
 public:
  HistorySyncOptinHandlerTest()
      : BrowserWithTestWindowTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    handler_ = std::make_unique<HistorySyncOptinHandler>(
        handler_remote_.BindNewPipeAndPassReceiver(), page_.BindAndGetRemote(),
        browser(), profile(), base::DoNothing());
  }

  void TearDown() override {
    handler_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  syncer::TestSyncService* test_sync_service() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(profile()));
  }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(profile());
  }

  AccountInfo SignInAndSetUpSyncService() {
    AccountInfo account_info = signin::MakeAccountAvailable(
        identity_manager(), signin::AccountAvailabilityOptionsBuilder()
                                .AsPrimary(signin::ConsentLevel::kSignin)
                                .WithGaiaId(GaiaId("gaia_id"))
                                .Build("test@gmail.com"));
    account_info.full_name = "fullname";
    account_info.given_name = "givenname";
    account_info.hosted_domain = "gmail.com";
    account_info.picture_url = "https://example.com";
    CHECK(account_info.IsValid());

    signin::UpdateAccountInfoForAccount(identity_manager(), account_info);

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
  testing::NiceMock<MockHistorySyncOptinPage> page_;
  mojo::Remote<history_sync_optin::mojom::PageHandler> handler_remote_;
  std::unique_ptr<HistorySyncOptinHandler> handler_;
  base::HistogramTester histogram_tester_;
};

// Tests that the handler sends the AccountInfo and ScreenMode when requested.
TEST_F(HistorySyncOptinHandlerTest, RequestScreenMode) {
  AccountInfo account_info = SignInAndSetUpSyncService();
  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
      true);
  signin::UpdateAccountInfoForAccount(identity_manager(), account_info);
  EXPECT_CALL(page_, SendAccountInfo(_)).Times(2);
  EXPECT_CALL(page_, SendScreenMode(ScreenMode::kUnrestricted)).Times(1);
  handler_->RequestAccountInfo();

  // The ScreenMode is only sent once.
  EXPECT_CALL(page_, SendScreenMode(ScreenMode::kRestricted)).Times(0);
  mutator.set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
      false);
  signin::UpdateAccountInfoForAccount(identity_manager(), account_info);
}

// Tests that the handler records ScreenMode::kUnrestricted metrics.
TEST_F(HistorySyncOptinHandlerTest, OnScreenModeUnrestricted) {
  AccountInfo account_info = SignInAndSetUpSyncService();
  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
      true);
  signin::UpdateAccountInfoForAccount(identity_manager(), account_info);
  EXPECT_CALL(page_, SendScreenMode(ScreenMode::kUnrestricted)).Times(1);
  handler_->RequestAccountInfo();

  histogram_tester_.ExpectUniqueSample(
      "Signin.SyncButtons.Shown",
      signin_metrics::SyncButtonsType::kSyncNotEqualWeighted, 1);
  histogram_tester_.ExpectTotalCount(
      "Signin.AccountCapabilities.UserVisibleLatency", 1);
  histogram_tester_.ExpectTotalCount("Signin.AccountCapabilities.FetchLatency",
                                     0);
  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountCapabilities.ImmediatelyAvailable", true, 1);
}

// Tests that the handler records ScreenMode::kUnrestricted metrics
// on accepting the History Sync dialog.
TEST_F(HistorySyncOptinHandlerTest, OnScreenModeUnrestrictedAccepted) {
  AccountInfo account_info = SignInAndSetUpSyncService();
  DisableAllSyncedDataTypes();
  EXPECT_FALSE(test_sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));

  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
      true);
  signin::UpdateAccountInfoForAccount(identity_manager(), account_info);

  handler_->RequestAccountInfo();
  handler_->Accept();
  EXPECT_TRUE(test_sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));

  histogram_tester_.ExpectUniqueSample(
      "Signin.SyncButtons.Clicked",
      signin_metrics::SyncButtonClicked::kHistorySyncOptInNotEqualWeighted, 1);
}

// Tests that the handler records ScreenMode::kUnrestricted metrics
// on rejecting the History Sync dialog.
TEST_F(HistorySyncOptinHandlerTest, OnScreenModeUnrestrictedRejected) {
  AccountInfo account_info = SignInAndSetUpSyncService();
  DisableAllSyncedDataTypes();
  EXPECT_FALSE(test_sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));

  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
      true);
  signin::UpdateAccountInfoForAccount(identity_manager(), account_info);

  handler_->RequestAccountInfo();
  handler_->Reject();
  EXPECT_FALSE(test_sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));

  histogram_tester_.ExpectUniqueSample(
      "Signin.SyncButtons.Clicked",
      signin_metrics::SyncButtonClicked::kHistorySyncCancelNotEqualWeighted, 1);
}

// Tests that the handler records ScreenMode::kRestricted metrics.
TEST_F(HistorySyncOptinHandlerTest, OnScreenModeRestricted) {
  AccountInfo account_info = SignInAndSetUpSyncService();
  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
      false);
  signin::UpdateAccountInfoForAccount(identity_manager(), account_info);
  EXPECT_CALL(page_, SendScreenMode(ScreenMode::kRestricted)).Times(1);
  handler_->RequestAccountInfo();

  histogram_tester_.ExpectUniqueSample(
      "Signin.SyncButtons.Shown",
      signin_metrics::SyncButtonsType::kSyncEqualWeightedFromCapability, 1);
  histogram_tester_.ExpectTotalCount(
      "Signin.AccountCapabilities.UserVisibleLatency", 1);
  histogram_tester_.ExpectTotalCount("Signin.AccountCapabilities.FetchLatency",
                                     0);
  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountCapabilities.ImmediatelyAvailable", true, 1);
}

// Tests that the handler records ScreenMode::kRestricted metrics
// on accepting the History Sync dialog.
TEST_F(HistorySyncOptinHandlerTest, OnScreenModeRestrictedAccepted) {
  AccountInfo account_info = SignInAndSetUpSyncService();
  DisableAllSyncedDataTypes();
  EXPECT_FALSE(test_sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));

  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
      false);
  signin::UpdateAccountInfoForAccount(identity_manager(), account_info);

  handler_->RequestAccountInfo();
  handler_->Accept();
  EXPECT_TRUE(test_sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));

  histogram_tester_.ExpectUniqueSample(
      "Signin.SyncButtons.Clicked",
      signin_metrics::SyncButtonClicked::kHistorySyncOptInEqualWeighted, 1);
}

// Tests that the handler records ScreenMode::kRestricted metrics
// on rejecting the History Sync dialog.
TEST_F(HistorySyncOptinHandlerTest, OnScreenModeRestrictedRejected) {
  AccountInfo account_info = SignInAndSetUpSyncService();
  DisableAllSyncedDataTypes();
  EXPECT_FALSE(test_sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));

  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
      false);
  signin::UpdateAccountInfoForAccount(identity_manager(), account_info);

  handler_->RequestAccountInfo();
  handler_->Reject();
  EXPECT_FALSE(test_sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));

  histogram_tester_.ExpectUniqueSample(
      "Signin.SyncButtons.Clicked",
      signin_metrics::SyncButtonClicked::kHistorySyncCancelEqualWeighted, 1);
}

// Tests that the handler send ScreenMode::kDeadlined if account capabilities
// are not available before the deadline.
TEST_F(HistorySyncOptinHandlerTest, OnScreenModeTimeout) {
  AccountInfo account_info = SignInAndSetUpSyncService();
  DisableAllSyncedDataTypes();
  EXPECT_FALSE(test_sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));

  handler_->RequestAccountInfo();
  EXPECT_CALL(page_, SendScreenMode(ScreenMode::kDeadlined));
  task_environment()->FastForwardBy(base::Seconds(1));

  handler_->Accept();
  EXPECT_TRUE(test_sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));

  histogram_tester_.ExpectUniqueSample(
      "Signin.SyncButtons.Shown",
      signin_metrics::SyncButtonsType::kSyncEqualWeightedFromDeadline, 1);
  histogram_tester_.ExpectUniqueSample(
      "Signin.SyncButtons.Clicked",
      signin_metrics::SyncButtonClicked::kHistorySyncOptInEqualWeighted, 1);
  histogram_tester_.ExpectTotalCount(
      "Signin.AccountCapabilities.UserVisibleLatency", 1);
  histogram_tester_.ExpectTotalCount("Signin.AccountCapabilities.FetchLatency",
                                     1);
  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountCapabilities.ImmediatelyAvailable", false, 1);
}

}  // namespace
