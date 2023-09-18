// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_STORAGE_SERVICE_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_STORAGE_SERVICE_H_

#include <ostream>
#include <set>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// Declare in the global namespace for test purposes.
class FeaturePromoStorageInteractiveTest;

namespace user_education {

// This service manages snooze and other display data for in-product help
// promos.
//
// It is an abstract base class in order to support multiple frameworks/
// platforms, and different data stores.
//
// Before showing an IPH, the IPH controller should ask if the IPH is blocked.
// The controller should also notify after the IPH is shown and after the user
// clicks the snooze/dismiss button.
class FeaturePromoStorageService {
 public:
  FeaturePromoStorageService();
  virtual ~FeaturePromoStorageService();

  // Disallow copy and assign.
  FeaturePromoStorageService(const FeaturePromoStorageService&) = delete;
  FeaturePromoStorageService& operator=(const FeaturePromoStorageService&) =
      delete;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // Most of these values are internal to the FeaturePromoController. Only
  // a few are made public to users (through FeaturePromoCloseReason) that
  // might call EndPromo in order to provide context for emitting metrics.
  enum CloseReason {
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
    kOverrideForPrecedence = 10,  // Promo aborted for higher priority Promo.

    kMaxValue = kOverrideForPrecedence,
  };

  // Dismissal and snooze information.
  struct PromoData {
    PromoData();
    ~PromoData();
    PromoData(const PromoData&);
    PromoData(PromoData&&);
    PromoData& operator=(const PromoData&);
    PromoData& operator=(PromoData&&);

    bool is_dismissed = false;
    CloseReason last_dismissed_by = CloseReason::kCancel;
    base::Time last_show_time = base::Time();
    base::Time last_snooze_time = base::Time();
    base::TimeDelta last_snooze_duration = base::TimeDelta();
    int snooze_count = 0;
    int show_count = 0;
    std::set<std::string> shown_for_apps;
  };

  virtual absl::optional<PromoData> ReadPromoData(
      const base::Feature& iph_feature) const = 0;

  virtual void SavePromoData(const base::Feature& iph_feature,
                             const PromoData& promo_data) = 0;

  // Reset the state of |iph_feature|.
  virtual void Reset(const base::Feature& iph_feature) = 0;

  // Returns the set of apps that `iph_feature` has been shown for.
  std::set<std::string> GetShownForApps(const base::Feature& iph_feature) const;

  // Returns the count of previous snoozes for `iph_feature`.
  int GetSnoozeCount(const base::Feature& iph_feature) const;
};

std::ostream& operator<<(std::ostream& oss,
                         FeaturePromoStorageService::CloseReason close_reason);

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_STORAGE_SERVICE_H_
