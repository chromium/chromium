// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/promo_cards/password_checkup_promo.h"

#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "ui/base/l10n/l10n_util.h"

constexpr base::TimeDelta kPasswordCheckupPromoPeriod = base::Days(7);
constexpr char kCheckupPromoId[] = "password_checkup_promo";

PasswordCheckupPromo::PasswordCheckupPromo(
    PrefService* prefs,
    extensions::PasswordsPrivateDelegate* delegate)
    : password_manager::PasswordPromoCardBase(kCheckupPromoId, prefs) {
  CHECK(delegate);
  delegate_ = delegate->AsWeakPtr();
}

PasswordCheckupPromo::~PasswordCheckupPromo() = default;

std::string PasswordCheckupPromo::GetPromoID() const {
  return kCheckupPromoId;
}

password_manager::PromoCardType PasswordCheckupPromo::GetPromoCardType() const {
  return password_manager::PromoCardType::kCheckup;
}

bool PasswordCheckupPromo::ShouldShowPromo() const {
  // Don't show promo if checkup is disabled by policy.
  if (!prefs_->GetBoolean(
          password_manager::prefs::kPasswordLeakDetectionEnabled)) {
    return false;
  }
  // Don't show promo if there are no saved passwords.
  if (!delegate_ || delegate_->GetCredentialGroups().empty()) {
    return false;
  }
  // If promo card was dismissed or shown already for
  // `kPromoDisplayLimit` times, show it in a week next time.
  bool should_suppress =
      was_dismissed_ ||
      number_of_times_shown_ >= PasswordPromoCardBase::kPromoDisplayLimit;
  return !should_suppress ||
         base::Time().Now() - last_time_shown_ > kPasswordCheckupPromoPeriod;
}

std::u16string PasswordCheckupPromo::GetTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_CHECKUP_PROMO_CARD_TITLE);
}

std::u16string PasswordCheckupPromo::GetDescription() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_CHECKUP_PROMO_CARD_DESCRIPTION);
}

std::u16string PasswordCheckupPromo::GetActionButtonText() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_CHECKUP_PROMO_CARD_ACTION);
}
