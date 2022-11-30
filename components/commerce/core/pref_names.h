// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_PREF_NAMES_H_
#define COMPONENTS_COMMERCE_CORE_PREF_NAMES_H_

class PrefRegistrySimple;

namespace commerce {

extern const char kCommerceDailyMetricsLastUpdateTime[];
extern const char kShoppingListBookmarkLastUpdateTime[];

// This setting is primarily for enabling or disabling the shopping list feature
// in enterprise settings.
extern const char kShoppingListEnabledPrefName[];

extern const char kWebAndAppActivityEnabledForShopping[];
extern const char kPriceEmailNotificationsEnabled[];

// Register preference names for commerce features.
void RegisterPrefs(PrefRegistrySimple* registry);

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_PREF_NAMES_H_