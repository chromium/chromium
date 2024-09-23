// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/promo_cards/web_password_manager_promo.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "ui/base/l10n/l10n_util.h"

constexpr char kWebPasswordManagerPromoId[] = "passwords_on_web_promo";

WebPasswordManagerPromo::WebPasswordManagerPromo(
    PrefService* prefs,
    const syncer::SyncService* sync_service)
    : password_manager::PasswordPromoCardBase(kWebPasswordManagerPromoId,
                                              prefs) {
  // TODO(crbug.com/40067296): Migrate away from `ConsentLevel::kSync` on
  // desktop platforms and remove #ifdef below.
#if BUILDFLAG(IS_ANDROID)
#error If this code is built on Android, please update TODO above.
#endif  // BUILDFLAG(IS_ANDROID)
  sync_enabled_ =
      password_manager::sync_util::IsSyncFeatureActiveIncludingPasswords(
          sync_service);
}

std::string WebPasswordManagerPromo::GetPromoID() const {
  return kWebPasswordManagerPromoId;
}

password_manager::PromoCardType WebPasswordManagerPromo::GetPromoCardType()
    const {
  return password_manager::PromoCardType::kWebPasswordManager;
}

bool WebPasswordManagerPromo::ShouldShowPromo() const {
  if (!sync_enabled_) {
    return false;
  }

  return !was_dismissed_ &&
         number_of_times_shown_ < PasswordPromoCardBase::kPromoDisplayLimit;
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
