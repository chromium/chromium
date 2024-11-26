// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/autofill/autofill_bubble_signin_promo_controller.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/test/test_sync_service.h"

namespace {

std::unique_ptr<KeyedService> BuildTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

}  // namespace

class AutofillBubbleSignInPromoControllerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override;

  void TearDown() override {
    sync_service_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  autofill::AutofillBubbleSignInPromoController* autofill_bubble_controller() {
    return autofill_bubble_controller_.get();
  }

  base::MockCallback<base::OnceCallback<void(content::WebContents*)>>
      mock_callback_;
  raw_ptr<syncer::TestSyncService> sync_service_;

 private:
  base::test::ScopedFeatureList feature_list_{
      switches::kExplicitBrowserSigninUIOnDesktop};
  std::unique_ptr<autofill::AutofillBubbleSignInPromoController>
      autofill_bubble_controller_;
};

void AutofillBubbleSignInPromoControllerTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  autofill_bubble_controller_ =
      std::make_unique<autofill::AutofillBubbleSignInPromoController>(
          *web_contents(),
          signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE,
          mock_callback_.Get());

  sync_service_ = static_cast<syncer::TestSyncService*>(
      SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating(&BuildTestSyncService)));
}

TEST_F(AutofillBubbleSignInPromoControllerTest,
       MovesPasswordUponSignInWithExistingAccountAndPasswordsEnabled) {
  // First, the sync service is inactive.
  sync_service_->SetMaxTransportState(
      syncer::SyncService::TransportState::CONFIGURING);

  // Simulate a sign in to the web.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());

  AccountInfo account_info = signin::MakeAccountAvailable(
      identity_manager,
      signin::AccountAvailabilityOptionsBuilder()
          .WithAccessPoint(signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN)
          .Build("test@gmail.com"));

  {
    // Check that the move callback will not be called until the sync service is
    // ready.
    EXPECT_CALL(mock_callback_, Run(web_contents())).Times(0);
    autofill_bubble_controller()->OnSignInToChromeClicked(account_info);
  }

  // Enable passwords for the sync service.
  sync_service_->SetMaxTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  sync_service_->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPasswords, true);

  // Check that the move callback will be called this time.
  EXPECT_CALL(mock_callback_, Run(web_contents()));

  // Now firing a state changed event should trigger a password move.
  sync_service_->FireStateChanged();

  // Check that the user was signed in to Chrome.
  EXPECT_TRUE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
}

TEST_F(AutofillBubbleSignInPromoControllerTest,
       DoesNotMovePasswordWhenSyncServiceIsInactive) {
  // Set the sync service as inactive.
  sync_service_->SetMaxTransportState(
      syncer::SyncService::TransportState::CONFIGURING);

  // Simulate a sign in to the web.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());

  AccountInfo account_info = signin::MakeAccountAvailable(
      identity_manager,
      signin::AccountAvailabilityOptionsBuilder()
          .WithAccessPoint(signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN)
          .Build("test@gmail.com"));

  // Check that the move callback will not be called.
  EXPECT_CALL(mock_callback_, Run(web_contents())).Times(0);

  // This should sign in the user to Chrome directly, as there is already an
  // account on the web.
  autofill_bubble_controller()->OnSignInToChromeClicked(account_info);

  // Also firing a state changed event should not trigger a password move.
  sync_service_->FireStateChanged();

  // Check that the user was signed in to Chrome.
  EXPECT_TRUE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
}

TEST_F(AutofillBubbleSignInPromoControllerTest,
       DoesNotMovePasswordUponSignInWithExistingAccountAndPasswordsDisabled) {
  // Disable passwords for the sync service, but make sure it is active.
  sync_service_->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPasswords, false);
  sync_service_->SetMaxTransportState(
      syncer::SyncService::TransportState::ACTIVE);

  // Simulate a sign in to the web.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());

  AccountInfo account_info = signin::MakeAccountAvailable(
      identity_manager,
      signin::AccountAvailabilityOptionsBuilder()
          .WithAccessPoint(signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN)
          .Build("test@gmail.com"));

  // Check that the move callback will not be called.
  EXPECT_CALL(mock_callback_, Run(web_contents())).Times(0);

  // This should sign in the user to Chrome directly, as there is already an
  // account on the web. It should not trigger a password move.
  autofill_bubble_controller()->OnSignInToChromeClicked(account_info);

  // Also firing a state changed event should not trigger a password move.
  sync_service_->FireStateChanged();

  // Check that the user was signed in to Chrome.
  EXPECT_TRUE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
}
