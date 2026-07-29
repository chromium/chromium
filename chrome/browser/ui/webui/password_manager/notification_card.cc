// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/notification_card.h"

#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/webui_url_constants.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync/service/sync_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace password_manager {

namespace {

constexpr char kIdKey[] = "id";
constexpr char kLastTimeShownKey[] = "last_time_shown";
constexpr char kNumberOfTimesShownKey[] = "number_of_times_shown";
constexpr char kWasDismissedKey[] = "was_dismissed";

// Creates new pref entry for the notification card with a given id.
base::DictValue CreateNotificationCardPrefEntry(const std::string& id) {
  base::DictValue notification_card_pref_entry;
  notification_card_pref_entry.Set(kIdKey, id);
  notification_card_pref_entry.Set(kLastTimeShownKey,
                                   base::TimeToValue(base::Time()));
  notification_card_pref_entry.Set(kNumberOfTimesShownKey, 0);
  notification_card_pref_entry.Set(kWasDismissedKey, false);
  return notification_card_pref_entry;
}

}  // namespace

// static

PasswordNotificationCardBase::PasswordNotificationCardBase(
    const std::string& id,
    PrefService* prefs)
    : prefs_(prefs) {
  const base::ListValue& notification_card_prefs =
      prefs_->GetList(prefs::kPasswordManagerPromoCardsList);
  for (const auto& notification_card_pref : notification_card_prefs) {
    auto* promo_id = notification_card_pref.GetDict().FindString(kIdKey);

    if (promo_id == nullptr || *promo_id != id) {
      continue;
    }

    number_of_times_shown_ =
        *notification_card_pref.GetDict().FindInt(kNumberOfTimesShownKey);
    last_time_shown_ = base::ValueToTime(notification_card_pref.GetDict().Find(
                                             kLastTimeShownKey))
                           .value();
    was_dismissed_ =
        *notification_card_pref.GetDict().FindBool(kWasDismissedKey);
    return;
  }
  // If there is no pref with matching ID, create one.
  ScopedListPrefUpdate update(prefs_, prefs::kPasswordManagerPromoCardsList);
  update.Get().Append(CreateNotificationCardPrefEntry(id));
}

PasswordNotificationCardBase::~PasswordNotificationCardBase() = default;

std::u16string PasswordNotificationCardBase::GetActionButtonText() const {
  return std::u16string();
}

void PasswordNotificationCardBase::OnNotificationCardDismissed() {
  was_dismissed_ = true;

  ScopedListPrefUpdate update(prefs_, prefs::kPasswordManagerPromoCardsList);
  for (auto& notification_card_pref : update.Get()) {
    if (*notification_card_pref.GetDict().FindString(kIdKey) == GetCardID()) {
      notification_card_pref.GetDict().Set(kWasDismissedKey, true);
      break;
    }
  }
}

void PasswordNotificationCardBase::OnNotificationCardShown() {
  number_of_times_shown_++;
  last_time_shown_ = base::Time::Now();

  ScopedListPrefUpdate update(prefs_, prefs::kPasswordManagerPromoCardsList);
  for (auto& notification_card_pref : update.Get()) {
    if (*notification_card_pref.GetDict().FindString(kIdKey) == GetCardID()) {
      notification_card_pref.GetDict().Set(kNumberOfTimesShownKey,
                                           number_of_times_shown_);
      notification_card_pref.GetDict().Set(kLastTimeShownKey,
                                           base::TimeToValue(last_time_shown_));
      break;
    }
  }
  base::UmaHistogramEnumeration("PasswordManager.PromoCard.Shown",
                                GetNotificationCardType());
}

}  // namespace password_manager
