// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/notification_cards/passkey_unlock_promo.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/webui/password_manager/notification_card.h"
#include "chrome/browser/webauthn/passkey_unlock_manager.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
constexpr char kPasskeyUnlockPromoId[] = "passkey_unlock_promo";
}  // namespace

PasskeyUnlockPromo::PasskeyUnlockPromo(
    webauthn::PasskeyUnlockManager* passkey_unlock_manager)
    : passkey_unlock_manager_(passkey_unlock_manager) {}

PasskeyUnlockPromo::~PasskeyUnlockPromo() = default;

std::string PasskeyUnlockPromo::GetCardID() const {
  return kPasskeyUnlockPromoId;
}

password_manager::NotificationCardType
PasskeyUnlockPromo::GetNotificationCardType() const {
  return password_manager::NotificationCardType::kPasskeyUnlock;
}

bool PasskeyUnlockPromo::ShouldShowCard(
    const password_manager::NotificationCardPrefState& pref_state) const {
  if (pref_state.was_dismissed) {
    return false;
  }

  if (!passkey_unlock_manager_ ||
      !passkey_unlock_manager_->ShouldDisplayErrorUi()) {
    return false;
  }

  return base::FeatureList::IsEnabled(
      password_manager::features::kPasskeyUnlockPromo);
}

std::u16string PasskeyUnlockPromo::GetTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_PASSKEYS_UNLOCK_PROMO_CARD_TITLE);
}

std::u16string PasskeyUnlockPromo::GetDescription() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_PASSKEYS_UNLOCK_PROMO_CARD_DESCRIPTION);
}

std::u16string PasskeyUnlockPromo::GetActionButtonText() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_PASSKEYS_UNLOCK_PROMO_CARD_ACTION);
}
