// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/promo_card.h"

#include "base/json/values_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace password_manager {

namespace {

constexpr char kIdKey[] = "id";
constexpr char kLastTimeShownKey[] = "last_time_shown";
constexpr char kNumberOfTimesShownKey[] = "number_of_times_shown";
constexpr char kWasDismissedKey[] = "was_dismissed";

// Creates new pref entry for the promo card with a given id.
base::Value::Dict CreatePromoCardPrefEntry(const std::string& id) {
  base::Value::Dict promo_card_pref_entry;
  promo_card_pref_entry.Set(kIdKey, id);
  promo_card_pref_entry.Set(kLastTimeShownKey, base::TimeToValue(base::Time()));
  promo_card_pref_entry.Set(kNumberOfTimesShownKey, 0);
  promo_card_pref_entry.Set(kWasDismissedKey, false);
  return promo_card_pref_entry;
}

}  // namespace

PromoCardInterface::PromoCardInterface(const std::string& id,
                                       PrefService* prefs)
    : prefs_(prefs) {
  const base::Value::List& promo_card_prefs =
      prefs_->GetList(prefs::kPasswordManagerPromoCardsList);
  for (const auto& promo_card_pref : promo_card_prefs) {
    if (*promo_card_pref.GetDict().FindString(kIdKey) == id) {
      number_of_times_shown_ =
          *promo_card_pref.GetDict().FindInt(kNumberOfTimesShownKey);
      last_time_shown_ =
          base::ValueToTime(promo_card_pref.GetDict().Find(kLastTimeShownKey))
              .value();
      was_dismissed_ = *promo_card_pref.GetDict().FindBool(kWasDismissedKey);
      return;
    }
  }
  // If there is no pref with matching ID, create one.
  ScopedListPrefUpdate update(prefs_, prefs::kPasswordManagerPromoCardsList);
  update.Get().Append(CreatePromoCardPrefEntry(id));
}

PromoCardInterface::~PromoCardInterface() = default;

std::u16string PromoCardInterface::GetActionButtonText() const {
  return std::u16string();
}

void PromoCardInterface::OnPromoCardDismissed() {
  was_dismissed_ = true;

  ScopedListPrefUpdate update(prefs_, prefs::kPasswordManagerPromoCardsList);
  for (auto& promo_card_pref : update.Get()) {
    if (*promo_card_pref.GetDict().FindString(kIdKey) == GetPromoID()) {
      promo_card_pref.GetDict().Set(kWasDismissedKey, true);
      return;
    }
  }
}

void PromoCardInterface::OnPromoCardShown() {
  number_of_times_shown_++;
  last_time_shown_ = base::Time::Now();

  ScopedListPrefUpdate update(prefs_, prefs::kPasswordManagerPromoCardsList);
  for (auto& promo_card_pref : update.Get()) {
    if (*promo_card_pref.GetDict().FindString(kIdKey) == GetPromoID()) {
      promo_card_pref.GetDict().Set(kNumberOfTimesShownKey,
                                    number_of_times_shown_);
      promo_card_pref.GetDict().Set(kLastTimeShownKey,
                                    base::TimeToValue(last_time_shown_));
      return;
    }
  }
}

}  // namespace password_manager
