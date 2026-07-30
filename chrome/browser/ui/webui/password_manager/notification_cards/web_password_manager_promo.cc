// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/notification_cards/web_password_manager_promo.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/sync/base/features.h"
#include "ui/base/l10n/l10n_util.h"

constexpr char kWebPasswordManagerPromoId[] = "passwords_on_web_promo";

WebPasswordManagerPromo::WebPasswordManagerPromo(
    const syncer::SyncService* sync_service) {
  sync_enabled_ =
      syncer::IsReplaceSyncPromosWithSignInPromosEnabled()
          ? password_manager::sync_util::GetPasswordSyncState(sync_service) !=
                password_manager::sync_util::SyncState::kNotActive
          : password_manager::sync_util::IsSyncFeatureActiveIncludingPasswords(
                sync_service);
}

std::string WebPasswordManagerPromo::GetCardID() const {
  return kWebPasswordManagerPromoId;
}

password_manager::NotificationCardType
WebPasswordManagerPromo::GetNotificationCardType() const {
  return password_manager::NotificationCardType::kWebPasswordManager;
}

bool WebPasswordManagerPromo::ShouldShowCard(
    const password_manager::NotificationCardPrefState& pref_state) const {
  if (!sync_enabled_) {
    return false;
  }

  return !pref_state.was_dismissed &&
         pref_state.number_of_times_shown <
             PasswordNotificationCardBase::kPromoDisplayLimit;
}

std::u16string WebPasswordManagerPromo::GetTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_WEB_PROMO_CARD_TITLE);
}

std::u16string WebPasswordManagerPromo::GetDescription() const {
  return l10n_util::GetStringFUTF16(
      IDS_PASSWORD_MANAGER_UI_WEB_PROMO_CARD_DESCRIPTION,
      base::ASCIIToUTF16(
          password_manager::kPasswordManagerAccountDashboardURL));
}
