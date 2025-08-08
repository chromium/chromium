// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_USER_EDUCATION_DATA_H_
#define COMPONENTS_USER_EDUCATION_COMMON_USER_EDUCATION_DATA_H_

#include <map>
#include <ostream>
#include <set>
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
struct UserEducationSessionData {
  UserEducationSessionData();
  UserEducationSessionData(const UserEducationSessionData&);
  UserEducationSessionData(UserEducationSessionData&&) noexcept;
  UserEducationSessionData& operator=(const UserEducationSessionData&);
  UserEducationSessionData& operator=(UserEducationSessionData&&) noexcept;
  ~UserEducationSessionData();

  // The beginning of the most recent session.
  base::Time start_time;

  // The last known time the browser was active.
  base::Time most_recent_active_time;

  // The (potentially non-monotonic but generally increasing) session number.
  // Any code which relies on this number should respond to an out-of-order
  // value as a cue to reset anything session-count-related, as it likely means
  // the user has reset session data from the User Education internals page.
  //
  // Note that zero is a null/invalid value and should never be returned from a
  // data read.
  int session_number = 1;
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

// Data used by the ProductMessagingController.
struct ProductMessagingData {
  ProductMessagingData();
  ProductMessagingData(const ProductMessagingData&);
  ProductMessagingData(ProductMessagingData&&) noexcept;
  ProductMessagingData& operator=(const ProductMessagingData&);
  ProductMessagingData& operator=(ProductMessagingData&&) noexcept;
  ~ProductMessagingData();

  // Notices that were shown this session.
  std::set<std::string> shown_notices;
};

// Data pertaining to a single NTP promo.
struct NtpPromoData {
  NtpPromoData();
  NtpPromoData(const NtpPromoData&);
  NtpPromoData(NtpPromoData&&) noexcept;
  NtpPromoData& operator=(const NtpPromoData&);
  NtpPromoData& operator=(NtpPromoData&&) noexcept;
  ~NtpPromoData();

  bool operator<=>(const NtpPromoData& other) const = default;

  // Time at which the promo was most recently clicked.
  base::Time last_clicked;

  // Time at which the promo was first seen to be complete.
  base::Time completed;

  // The session in which this promo was last shown in the top spot.
  int last_top_spot_session = 0;

  // The number of session this promo has been shown in the top spot, since
  // it most recently claimed the top spot. When reclaiming top spot,
  // this value must be reset.
  int top_spot_session_count = 0;
};

using KeyedNtpPromoDataMap = std::map<std::string, NtpPromoData>;

// Data pertaining to the NTP promo system as a whole.
struct NtpPromoPreferences {
  NtpPromoPreferences();
  NtpPromoPreferences(const NtpPromoPreferences&);
  NtpPromoPreferences(NtpPromoPreferences&&) noexcept;
  NtpPromoPreferences& operator=(const NtpPromoPreferences&);
  NtpPromoPreferences& operator=(NtpPromoPreferences&&) noexcept;
  ~NtpPromoPreferences();

  bool operator<=>(const NtpPromoPreferences& other) const = default;

  // Snooze timer.
  base::Time last_snoozed;

  // Whether all promos are disabled.
  bool disabled = false;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_USER_EDUCATION_DATA_H_
