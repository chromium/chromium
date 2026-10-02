// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/notification_cards/account_aware_password_signin_promo.h"

#include <algorithm>
#include <optional>

#include "base/feature_list.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_preview_data_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/signin/account_preview_utils.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_preview_data_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_prefs.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
constexpr char kAccountAwarePasswordSigninPromoId[] =
    "account_aware_password_signin_promo";
}  // namespace

AccountAwarePasswordSigninPromo::AccountAwarePasswordSigninPromo(
    Profile* profile)
    : profile_(profile) {
  CHECK(profile_);
}

AccountAwarePasswordSigninPromo::~AccountAwarePasswordSigninPromo() = default;

std::string AccountAwarePasswordSigninPromo::GetCardID() const {
  return kAccountAwarePasswordSigninPromoId;
}

password_manager::NotificationCardType
AccountAwarePasswordSigninPromo::GetNotificationCardType() const {
  return password_manager::NotificationCardType::kAccountAwarePasswordSignin;
}

password_manager::NotificationSeverity
AccountAwarePasswordSigninPromo::GetNotificationSeverity() const {
  return password_manager::NotificationSeverity::kHighPriorityPromo;
}

bool AccountAwarePasswordSigninPromo::ShouldShowCard(
    const password_manager::NotificationCardPrefState& pref_state) const {
  if (pref_state.was_dismissed ||
      pref_state.number_of_times_shown >=
          PasswordNotificationCardBase::kPromoDisplayLimit) {
    return false;
  }

  if (!base::FeatureList::IsEnabled(
          switches::kEnableAccountPreviewPreferredAccountFollowup)) {
    return false;
  }

  PrefService* pref_service = profile_->GetPrefs();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  syncer::SyncService* sync_service =
      SyncServiceFactory::IsSyncAllowed(profile_)
          ? SyncServiceFactory::GetForProfile(profile_)
          : nullptr;
  signin::AccountPreviewDataService* account_preview_data_service =
      AccountPreviewDataServiceFactory::GetForProfile(profile_);

  if (!pref_service || !identity_manager || !sync_service ||
      !account_preview_data_service ||
      sync_service->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY) ||
      syncer::SyncPrefs(pref_service).IsLocalSyncEnabled() ||
      !pref_service->GetBoolean(prefs::kSigninAllowed)) {
    return false;
  }

  if (sync_service->GetUserSettings()->IsTypeManagedByPolicy(
          syncer::UserSelectableType::kPasswords) ||
      !sync_service->GetDataTypesForTransportOnlyMode().Has(
          syncer::PASSWORDS)) {
    return false;
  }

  if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return false;
  }

  AccountInfo account_info = GetAccountForPromo();
  if (account_info.IsEmpty()) {
    return false;
  }

  std::optional<signin::AccountPreviewDataService::AccountPreviewPreference>
      preferred_account =
          account_preview_data_service->GetPreferredAccountForPromo();
  if (!preferred_account.has_value() ||
      preferred_account->gaia_id != account_info.GetGaiaId()) {
    return false;
  }

  return std::ranges::any_of(preferred_account->preferred_data_types,
                             [](const signin::PreferredDataTypeInfo& info) {
                               return info.data_type == syncer::PASSWORDS &&
                                      info.quartile !=
                                          signin::SyncDataQuartile::kZero;
                             });
}

std::u16string AccountAwarePasswordSigninPromo::GetTitle() const {
  std::optional<signin::AccountPreviewDataService::AccountPreviewPreference>
      preferred_account =
          AccountPreviewDataServiceFactory::GetForProfile(profile_)
              ->GetPreferredAccountForPromo();
  CHECK(preferred_account.has_value());
  return signin::GetAccountPreviewPasswordManagerPromoTitle(*preferred_account);
}

std::u16string AccountAwarePasswordSigninPromo::GetDescription() const {
  return l10n_util::GetStringFUTF16(
      IDS_PASSWORD_MANAGER_UI_SIGNIN_PROMO_CARD_DESCRIPTION,
      base::UTF8ToUTF16(base::EscapeForHTML(GetAccountForPromo().GetEmail())));
}

std::u16string AccountAwarePasswordSigninPromo::GetActionButtonText() const {
  AccountInfo account_info = GetAccountForPromo();
  return l10n_util::GetStringFUTF16(
      IDS_PROFILES_DICE_WEB_ONLY_SIGNIN_BUTTON,
      base::UTF8ToUTF16(
          account_info.GetGivenName().value_or(account_info.GetEmail())));
}

std::string AccountAwarePasswordSigninPromo::GetActionButtonAvatarUrl() const {
  return signin::GetAccountPictureUrl(GetAccountForPromo());
}

AccountInfo AccountAwarePasswordSigninPromo::GetAccountForPromo() const {
  return signin_ui_util::GetSingleAccountForPromos(
      IdentityManagerFactory::GetForProfile(profile_),
      AccountPreviewDataServiceFactory::GetForProfile(profile_));
}
