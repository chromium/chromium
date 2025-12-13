// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_PREF_NAMES_H_
#define COMPONENTS_COMMERCE_CORE_PREF_NAMES_H_

namespace commerce {

// keep-sorted start block=yes newline_separated=yes skip_lines=1

inline constexpr char kCommerceDailyMetricsLastUpdateTime[] =
    "commerce_daily_metrics_last_update_time";

inline constexpr char kPriceEmailNotificationsEnabled[] =
    "price_tracking.email_notifications_enabled";

// The pref that stores if the Price Tracking Home Module is enabled.
inline constexpr char kPriceTrackingHomeModuleEnabled[] =
    "home.module.price_tracking.enabled";

inline constexpr char kProductSpecificationsAcceptedDisclosureVersion[] =
    "product_specifications.accepted_disclosure_version";

inline constexpr char kProductSpecificationsEntryPointLastDismissedTime[] =
    "product_specifications_entry_point_last_dismissed_time";

inline constexpr char kProductSpecificationsEntryPointShowIntervalInDays[] =
    "product_specifications_entry_point_show_interval_in_days";

inline constexpr char kShoppingListBookmarkLastUpdateTime[] =
    "shopping_list_bookmark_last_update_time";

// This setting is primarily for enabling or disabling the shopping list feature
// in enterprise settings.
inline constexpr char kShoppingListEnabledPrefName[] = "shopping_list_enabled";

// keep-sorted end

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_PREF_NAMES_H_
