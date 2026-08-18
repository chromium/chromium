// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/signin/signin_utils_handler.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/account_preview_data_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/service/sync_service.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace {

signin_metrics::AccessPoint GetAccessPoint(
    signin::mojom::ChromeSigninAccessPoint access_point) {
  switch (access_point) {
    case signin::mojom::ChromeSigninAccessPoint::SETTINGS:
      return signin_metrics::AccessPoint::kSettings;
    case signin::mojom::ChromeSigninAccessPoint::SETTINGS_YOUR_SAVED_INFO:
      return signin_metrics::AccessPoint::kSettingsYourSavedInfo;
  }
  NOTREACHED();
}

bool IsChangePrimaryAccountAllowed(Profile* profile, const std::string& email) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  if (ChromeSigninClientFactory::GetForProfile(profile)
          ->IsClearPrimaryAccountAllowed() ||
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return true;
  }

  return gaia::AreEmailsSame(
      email,
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .email);
}

}  // namespace

SigninUtilsHandler::SigninUtilsHandler(
    mojo::PendingReceiver<signin::mojom::SigninPageHandler> receiver,
    Profile* profile)
    : receiver_(this, std::move(receiver)), profile_(profile) {
  CHECK(profile_);
}

SigninUtilsHandler::~SigninUtilsHandler() = default;

void SigninUtilsHandler::StartSignin(
    signin::mojom::ChromeSigninAccessPoint access_point) {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  CHECK(identity_manager);
  signin_metrics::AccessPoint signin_access_point =
      GetAccessPoint(access_point);

  // Should only be called if the user is not already signed in, has a auth
  // error, or a unrecoverable sync error requiring re-auth.
  signin_util::SignedInState state =
      signin_util::GetSignedInState(identity_manager);
  CHECK(state != signin_util::SignedInState::kSignedIn &&
        state != signin_util::SignedInState::kSyncing);

  syncer::SyncService* service = SyncServiceFactory::GetForProfile(profile_);

  // When the user has an unrecoverable error, they first have to sign out and
  // then sign in again.
  // Note: this sets the consent level to `signin::ConsentLevel::kSignin`.
  if (service && service->HasUnrecoverableError() &&
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    identity_manager->GetPrimaryAccountMutator()->RevokeSyncConsent(
        signin_metrics::ProfileSignout::kRevokeSyncFromSettings);
  }

  // If the identity manager already has a primary account, this is a
  // re-auth scenario, and we need to ensure that the user signs in with the
  // same email address.
  if (state == signin_util::SignedInState::kSyncPaused ||
      state == signin_util::SignedInState::kSignInPending) {
    SigninErrorController* error_controller =
        SigninErrorControllerFactory::GetForProfile(profile_);
    CHECK(error_controller->HasError());

    signin_ui_util::ShowReauthForPrimaryAccountWithAuthError(
        profile_, signin_access_point);
    return;
  }

  CHECK(IsChangePrimaryAccountAllowed(profile_, /*email=*/std::string()))
      << "Primary account already set and change is not allowed";
  signin_ui_util::EnableSyncFromSingleAccountPromo(profile_, CoreAccountInfo(),
                                                   signin_access_point);
}

void SigninUtilsHandler::SigninWithAccount(
    signin::mojom::ChromeSigninAccessPoint access_point,
    const std::string& email,
    bool is_default_promo_account) {
  CHECK(AccountConsistencyModeManager::IsDiceEnabledForProfile(profile_));
  signin_metrics::AccessPoint signin_access_point =
      GetAccessPoint(access_point);

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);

  signin_util::SignedInState state =
      signin_util::GetSignedInState(identity_manager);
  CHECK(state == signin_util::SignedInState::kWebOnlySignedIn);

  const AccountInfo maybe_account =
      identity_manager->FindExtendedAccountInfoByEmailAddress(email);
  signin_ui_util::EnableSyncFromMultiAccountPromo(
      profile_, maybe_account, signin_access_point, is_default_promo_account);
}

void SigninUtilsHandler::RecordSigninPendingOffered() {
  signin_metrics::LogSigninPendingOffered(
      signin_metrics::AccessPoint::kSettings);
}

void SigninUtilsHandler::RecordSigninOffered(
    signin::mojom::ChromeSigninAccessPoint access_point) {
  signin_metrics::AccessPoint signin_access_point =
      GetAccessPoint(access_point);

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  CHECK(identity_manager);
  signin_metrics::PromoAction promo_action =
      signin_ui_util::GetSingleAccountForPromos(
          identity_manager,
          AccountPreviewDataServiceFactory::GetForProfile(profile_))
              .IsEmpty()
          ? signin_metrics::PromoAction::
                PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT
          : signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT;

  signin_metrics::LogSignInOffered(signin_access_point, promo_action);
}
