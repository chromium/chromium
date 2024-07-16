// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/promo_cards/move_passwords_promo.h"

#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_utils.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
constexpr base::TimeDelta kMovePasswordsPromoPeriod = base::Days(7);

using extensions::api::passwords_private::CredentialGroup;
using extensions::api::passwords_private::PasswordUiEntry;

constexpr char kMovePasswordsId[] = "move_passwords_promo";

syncer::SyncService* GetSyncService(Profile* profile) {
  return SyncServiceFactory::IsSyncAllowed(profile)
             ? SyncServiceFactory::GetForProfile(profile)
             : nullptr;
}

// Checks if there are passwords saved only to this device.
bool HasLocalPasswords(extensions::PasswordsPrivateDelegate* delegate) {
  if (!delegate) {
    return false;
  }

  auto is_entry_saved_locally = [](const PasswordUiEntry& entry) {
    return entry.stored_in == extensions::api::passwords_private::
                                  PasswordStoreSet::kDeviceAndAccount ||
           entry.stored_in ==
               extensions::api::passwords_private::PasswordStoreSet::kDevice;
  };

  auto has_credential_group_with_local_passwords =
      [&is_entry_saved_locally](const CredentialGroup& credential_group) {
        return std::ranges::any_of(credential_group.entries,
                                   is_entry_saved_locally);
      };

  return std::ranges::any_of(delegate->GetCredentialGroups(),
                             has_credential_group_with_local_passwords);
}

std::u16string GetPrimaryAccountEmailFromProfile(Profile* profile) {
  if (!profile) {
    return std::u16string();
  }
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    return std::u16string();
  }
  return base::UTF8ToUTF16(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .email);
}

}  // namespace

MovePasswordsPromo::MovePasswordsPromo(
    Profile* profile,
    extensions::PasswordsPrivateDelegate* delegate)
    : password_manager::PasswordPromoCardBase(kMovePasswordsId,
                                              profile->GetPrefs()),
      profile_(profile) {
  CHECK(delegate);
  delegate_ = delegate->AsWeakPtr();
}

MovePasswordsPromo::~MovePasswordsPromo() = default;

std::string MovePasswordsPromo::GetPromoID() const {
  return kMovePasswordsId;
}

password_manager::PromoCardType MovePasswordsPromo::GetPromoCardType() const {
  return password_manager::PromoCardType::kMovePasswords;
}

bool MovePasswordsPromo::ShouldShowPromo() const {
  CHECK(profile_);
  syncer::SyncService* sync_service = GetSyncService(profile_);
  if (!sync_service ||
      !password_manager::features_util::IsOptedInForAccountStorage(
          profile_->GetPrefs(), sync_service)) {
    return false;
  }

  // If promo card was dismissed or shown already for
  // `kPromoDisplayLimit` times, show it in a week next time.
  bool should_suppress =
      was_dismissed_ ||
      number_of_times_shown_ >= PasswordPromoCardBase::kPromoDisplayLimit;

  bool bubble_is_not_over_prompted =
      !should_suppress ||
      base::Time().Now() - last_time_shown_ > kMovePasswordsPromoPeriod;

  return bubble_is_not_over_prompted && HasLocalPasswords(delegate_.get());
}

std::u16string MovePasswordsPromo::GetTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_MOVE_PASSWORDS_PROMO_CARD_TITLE);
}

std::u16string MovePasswordsPromo::GetDescription() const {
  CHECK(profile_);
  return l10n_util::GetStringFUTF16(
      IDS_PASSWORD_MANAGER_UI_MOVE_PASSWORDS_PROMO_CARD_DESCRIPTION,
      GetPrimaryAccountEmailFromProfile(profile_));
}

std::u16string MovePasswordsPromo::GetActionButtonText() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_MOVE_PASSWORDS_PROMO_CARD_ACTION_BUTTON);
}
