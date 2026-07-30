// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/notification_cards/relaunch_chrome_banner.h"

#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "ui/base/l10n/l10n_util.h"

constexpr char kRelaunchChromeId[] = "relaunch_chrome_promo";

RelaunchChromeBanner::RelaunchChromeBanner() = default;

RelaunchChromeBanner::~RelaunchChromeBanner() = default;

std::string RelaunchChromeBanner::GetCardID() const {
  return kRelaunchChromeId;
}

password_manager::NotificationCardType
RelaunchChromeBanner::GetNotificationCardType() const {
  return password_manager::NotificationCardType::kRelaunchChrome;
}

password_manager::NotificationSeverity
RelaunchChromeBanner::GetNotificationSeverity() const {
  return password_manager::NotificationSeverity::kCritical;
}

bool RelaunchChromeBanner::ShouldShowCard(
    const password_manager::NotificationCardPrefState& pref_state) const {
  if (is_encryption_available_.value_or(true)) {
    return false;
  }

  return base::FeatureList::IsEnabled(
      password_manager::features::kRestartToGainAccessToKeychain);
}

bool RelaunchChromeBanner::IsDismissible() const {
  return false;
}

std::u16string RelaunchChromeBanner::GetTitle() const {
  return l10n_util::GetStringUTF16(
#if BUILDFLAG(IS_MAC)
      IDS_PASSWORD_MANAGER_UI_RELAUNCH_CHROME_PROMO_CARD_TITLE
#elif BUILDFLAG(IS_LINUX)
      IDS_PASSWORD_MANAGER_UI_RELAUNCH_CHROME_PROMO_CARD_TITLE_LINUX
#endif
  );
}

std::u16string RelaunchChromeBanner::GetDescription() const {
  return l10n_util::GetStringUTF16(
#if BUILDFLAG(IS_MAC)
      IDS_PASSWORD_MANAGER_UI_RELAUNCH_CHROME_PROMO_CARD_DESCRIPTION
#elif BUILDFLAG(IS_LINUX)
      IDS_PASSWORD_MANAGER_UI_RELAUNCH_CHROME_PROMO_CARD_DESCRIPTION_LINUX
#endif
  );
}

std::u16string RelaunchChromeBanner::GetActionButtonText() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_RELAUNCH_CHROME_PROMO_CARD_ACTION);
}
