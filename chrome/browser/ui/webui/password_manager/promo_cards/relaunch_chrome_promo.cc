// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/promo_cards/relaunch_chrome_promo.h"

#include "chrome/grit/generated_resources.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "ui/base/l10n/l10n_util.h"

constexpr char kRelauchChromeId[] = "relaunch_chrome_promo";

RelaunchChromePromo::RelaunchChromePromo(PrefService* prefs)
    : password_manager::PasswordPromoCardBase(kRelauchChromeId, prefs) {}

RelaunchChromePromo::~RelaunchChromePromo() = default;

std::string RelaunchChromePromo::GetPromoID() const {
  return kRelauchChromeId;
}

password_manager::PromoCardType RelaunchChromePromo::GetPromoCardType() const {
  return password_manager::PromoCardType::kRelauchChrome;
}

bool RelaunchChromePromo::ShouldShowPromo() const {
  if (OSCrypt::IsEncryptionAvailable()) {
    return false;
  }

  return base::FeatureList::IsEnabled(
      password_manager::features::kRestartToGainAccessToKeychain);
}

std::u16string RelaunchChromePromo::GetTitle() const {
  return l10n_util::GetStringUTF16(
#if BUILDFLAG(IS_MAC)
      IDS_PASSWORD_MANAGER_UI_RELAUNCH_CHROME_PROMO_CARD_TITLE
#elif BUILDFLAG(IS_LINUX)
      IDS_PASSWORD_MANAGER_UI_RELAUNCH_CHROME_PROMO_CARD_TITLE_LINUX
#endif
  );
}

std::u16string RelaunchChromePromo::GetDescription() const {
  return l10n_util::GetStringUTF16(
#if BUILDFLAG(IS_MAC)
      IDS_PASSWORD_MANAGER_UI_RELAUNCH_CHROME_PROMO_CARD_DESCRIPTION
#elif BUILDFLAG(IS_LINUX)
      IDS_PASSWORD_MANAGER_UI_RELAUNCH_CHROME_PROMO_CARD_DESCRIPTION_LINUX
#endif
  );
}

std::u16string RelaunchChromePromo::GetActionButtonText() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_RELAUNCH_CHROME_PROMO_CARD_ACTION);
}
