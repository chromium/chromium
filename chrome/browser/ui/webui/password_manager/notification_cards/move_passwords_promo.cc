// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/notification_cards/move_passwords_promo.h"

#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/sync/service/sync_service.h"
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
// TODO(crbug.com/410001569): The dialog now shows the Batch Upload dialog,
// which uses the sync service to show the local data. Align whether or not
// promo is shown and the content shown in the dialog to use the same API: the
// sync service API.
int GetLocalPasswordsCount(extensions::PasswordsPrivateDelegate* delegate) {
  if (!delegate) {
    return 0;
  }

  auto is_entry_saved_locally = [](const PasswordUiEntry& entry) {
    return entry.stored_in == extensions::api::passwords_private::
                                  PasswordStoreSet::kDeviceAndAccount ||
           entry.stored_in ==
               extensions::api::passwords_private::PasswordStoreSet::kDevice;
  };

  int local_passwords_count = 0;
  for (const auto& credential_group : delegate->GetCredentialGroups()) {
    local_passwords_count +=
        std::ranges::count_if(credential_group.entries, is_entry_saved_locally);
  }

  return local_passwords_count;
}

}  // namespace

MovePasswordsPromo::MovePasswordsPromo(
    Profile* profile,
    extensions::PasswordsPrivateDelegate* delegate)
    : profile_(profile) {
  CHECK(delegate);
  delegate_ = delegate->AsWeakPtr();
}

MovePasswordsPromo::~MovePasswordsPromo() = default;

std::string MovePasswordsPromo::GetCardID() const {
  return kMovePasswordsId;
}

password_manager::NotificationCardType
MovePasswordsPromo::GetNotificationCardType() const {
  return password_manager::NotificationCardType::kMovePasswords;
}

bool MovePasswordsPromo::ShouldShowCard(
    const password_manager::NotificationCardPrefState& pref_state) const {
  CHECK(profile_);
  syncer::SyncService* sync_service = GetSyncService(profile_);
  if (!sync_service ||
      !password_manager::features_util::IsAccountStorageActive(sync_service) ||
      !sync_service->IsEngineInitialized()) {
    return false;
  }

  // If notification card was dismissed or shown already for
  // `kPromoDisplayLimit` times, show it in a week next time.
  bool should_suppress = pref_state.was_dismissed ||
                         pref_state.number_of_times_shown >=
                             PasswordNotificationCardBase::kPromoDisplayLimit;

  bool bubble_is_not_over_prompted =
      !should_suppress || base::Time::Now() - pref_state.last_time_shown >
                              kMovePasswordsPromoPeriod;

  return bubble_is_not_over_prompted &&
         GetLocalPasswordsCount(delegate_.get()) > 0;
}

std::u16string MovePasswordsPromo::GetTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_BATCH_UPLOAD_PROMO_CARD_TITLE);
}

std::u16string MovePasswordsPromo::GetDescription() const {
  return l10n_util::GetPluralStringFUTF16(
      IDS_BATCH_UPLOAD_SUBTITLE_DESCRIPTION_PASSWORDS_COMBO,
      GetLocalPasswordsCount(delegate_.get()));
}

std::u16string MovePasswordsPromo::GetActionButtonText() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_BATCH_UPLOAD_PROMO_CARD_ACTION_BUTTON);
}
