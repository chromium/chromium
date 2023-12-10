// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/pref_names.h"

#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"

namespace commerce {

const char kCommerceDailyMetricsLastUpdateTime[] =
    "commerce_daily_metrics_last_update_time";
const char kShoppingListBookmarkLastUpdateTime[] =
    "shopping_list_bookmark_last_update_time";
const char kShoppingListEnabledPrefName[] = "shopping_list_enabled";
const char kPriceEmailNotificationsEnabled[] =
    "price_tracking.email_notifications_enabled";

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      kPriceEmailNotificationsEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterTimePref(kCommerceDailyMetricsLastUpdateTime, base::Time());
  registry->RegisterTimePref(kShoppingListBookmarkLastUpdateTime, base::Time());

  registry->RegisterBooleanPref(kShoppingListEnabledPrefName, true);
}

}  // namespace commerce