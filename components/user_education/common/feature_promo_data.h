// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_DATA_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_DATA_H_

#include <map>
#include <ostream>
#include <string>

#include "base/time/time.h"

namespace user_education {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Represents the reason that a promo was ended/promo bubble was closed.
enum class FeaturePromoClosedReason {
  // Actions within the FeaturePromo.
  kDismiss = 0,  // Promo dismissed by user.
  kSnooze = 1,   // Promo snoozed by user.
  kAction = 2,   // Custom action taken by user.
  kCancel = 3,   // Promo was cancelled.

  // Actions outside the FeaturePromo.
  kTimeout = 4,         // Promo timed out.
  kAbortPromo = 5,      // Promo aborted by indirect user action.
  kFeatureEngaged = 6,  // Promo closed by indirect user engagement.

  // Controller system actions.
  kOverrideForUIRegionConflict = 7,  // Promo aborted to avoid overlap.
  kOverrideForDemo = 8,              // Promo aborted by the demo system.
  kOverrideForTesting = 9,           // Promo aborted for tests.
  kOverrideForPrecedence = 10,       // Promo aborted for higher priority Promo.

  // Additional ways a promo can be aborted.
  kAbortedByFeature = 11,          // EndPromo() explicitly called.
  kAbortedByAnchorHidden = 12,     // Anchor element disappeared.
  kAbortedByBubbleDestroyed = 13,  // HelpBubble object destroyed.

  kMaxValue = kAbortedByBubbleDestroyed,
};

std::ostream& operator<<(std::ostream& oss,
                         FeaturePromoClosedReason close_reason);

// Per-key dismissal information.
struct KeyedFeaturePromoData {
  KeyedFeaturePromoData() = default;
  KeyedFeaturePromoData(const KeyedFeaturePromoData&) = default;
  KeyedFeaturePromoData(KeyedFeaturePromoData&&) noexcept = default;
  KeyedFeaturePromoData& operator=(const KeyedFeaturePromoData&) = default;
  KeyedFeaturePromoData& operator=(KeyedFeaturePromoData&&) noexcept = default;
  ~KeyedFeaturePromoData() = default;

  bool operator<=>(const KeyedFeaturePromoData& other) const = default;

  int show_count = 0;
  base::Time last_shown_time;
};

using KeyedFeaturePromoDataMap = std::map<std::string, KeyedFeaturePromoData>;

// Dismissal and snooze information.
struct FeaturePromoData {
  FeaturePromoData();
  FeaturePromoData(const FeaturePromoData&);
  FeaturePromoData(FeaturePromoData&&) noexcept;
  FeaturePromoData& operator=(const FeaturePromoData&);
  FeaturePromoData& operator=(FeaturePromoData&&) noexcept;
  ~FeaturePromoData();

  bool is_dismissed = false;
  FeaturePromoClosedReason last_dismissed_by =
      FeaturePromoClosedReason::kCancel;
  base::Time first_show_time;
  base::Time last_show_time;
  base::Time last_snooze_time;
  int snooze_count = 0;
  int show_count = 0;
  int promo_index = 0;
  KeyedFeaturePromoDataMap shown_for_keys;
};

// Data about the current session, which can persist across browser restarts.
struct FeaturePromoSessionData {
  FeaturePromoSessionData();
  FeaturePromoSessionData(const FeaturePromoSessionData&);
  FeaturePromoSessionData(FeaturePromoSessionData&&) noexcept;
  FeaturePromoSessionData& operator=(const FeaturePromoSessionData&);
  FeaturePromoSessionData& operator=(FeaturePromoSessionData&&) noexcept;
  ~FeaturePromoSessionData();

  // The beginning of the most recent session.
  base::Time start_time;

  // The last known time the browser was active.
  base::Time most_recent_active_time;
};

// Data that must be kept across browser restart to support the feature promo
// policy.
struct FeaturePromoPolicyData {
  FeaturePromoPolicyData();
  FeaturePromoPolicyData(const FeaturePromoPolicyData&);
  FeaturePromoPolicyData(FeaturePromoPolicyData&&) noexcept;
  FeaturePromoPolicyData& operator=(const FeaturePromoPolicyData&);
  FeaturePromoPolicyData& operator=(FeaturePromoPolicyData&&) noexcept;
  ~FeaturePromoPolicyData();

  // The time of the last heavyweight promotion the user saw
  base::Time last_heavyweight_promo_time;
};

// Data about the "New" Badge for a particular feature.
struct NewBadgeData {
  // The number of times the "New" Badge has been shown.
  int show_count = 0;

  // The number of times the promoted entry point has been used.
  int used_count = 0;

  // The first time the promoted feature is enabled.
  base::Time feature_enabled_time;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_DATA_H_
