// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/notification_cards/password_manager_shortcut_promo.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "ui/base/l10n/l10n_util.h"

constexpr char kShortcutPromoId[] = "password_shortcut_promo";

PasswordManagerShortcutPromo::PasswordManagerShortcutPromo(Profile* profile)
    : profile_(profile) {
  is_shortcut_installed_ =
      web_app::FindInstalledAppWithUrlInScope(
          profile, GURL(chrome::kChromeUIPasswordManagerURL))
          .has_value();
}

std::string PasswordManagerShortcutPromo::GetCardID() const {
  return kShortcutPromoId;
}

password_manager::NotificationCardType
PasswordManagerShortcutPromo::GetNotificationCardType() const {
  return password_manager::NotificationCardType::kAddShortcut;
}

bool PasswordManagerShortcutPromo::ShouldShowCard(
    const password_manager::NotificationCardPrefState& pref_state) const {
  if (is_shortcut_installed_ || !web_app::AreWebAppsEnabled(profile_)) {
    return false;
  }

  auto* service = UserEducationServiceFactory::GetForBrowserContext(profile_);
  if (service) {
    auto* tutorial_service = service->tutorial_service();
    if (tutorial_service && tutorial_service->IsRunningTutorial()) {
      return false;
    }
  }

  return !pref_state.was_dismissed &&
         pref_state.number_of_times_shown <
             PasswordNotificationCardBase::kPromoDisplayLimit;
}

std::u16string PasswordManagerShortcutPromo::GetTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_SHORTCUT_PROMO_CARD_TITLE);
}

std::u16string PasswordManagerShortcutPromo::GetDescription() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_SHORTCUT_PROMO_CARD_DESCRIPTION);
}

std::u16string PasswordManagerShortcutPromo::GetActionButtonText() const {
  return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_UI_ADD_SHORTCUT_TITLE);
}
