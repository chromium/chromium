// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_SNOOZE_SERVICE_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_SNOOZE_SERVICE_H_

#include "base/feature_list.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// Declare in the global namespace for test purposes.
class FeaturePromoSnoozeInteractiveTest;

namespace user_education {

// This service manages snooze and dismiss of snoozable in-product help promo.
// It is an abstract base class in order to support multiple frameworks/
// platforms and potentially to migrate the backing to the new snooze feature
// of the feature engagement tracker.
//
// Before showing an IPH, the IPH controller should ask if the IPH is blocked.
// The controller should also notify after the IPH is shown and after the user
// clicks the snooze/dismiss button.
class FeaturePromoSnoozeService {
 public:
  // Policies to handler users who don't interact with the IPH.
  enum class NonClickerPolicy {
    // Permanently dismiss the IPH. Equivalent to clicking the dismiss button.
    kDismiss = 0,
    // Reshow the IPH later after at least 14 days.
    kLongSnooze
  };

  // Maximum count of snoozes to track in UMA histogram.
  // Snooze counts that are equal or larger than this value will be conflated.
  static constexpr int kUmaMaxSnoozeCount = 10;

  // The snooze duration defaults to 1 day plus 2 additional hours in hope to
  // stagger busy hours in the days.
  static constexpr base::TimeDelta kDefaultSnoozeDuration = base::Hours(26);

  FeaturePromoSnoozeService();
  virtual ~FeaturePromoSnoozeService();

  // Disallow copy and assign.
  FeaturePromoSnoozeService(const FeaturePromoSnoozeService&) = delete;
  FeaturePromoSnoozeService& operator=(const FeaturePromoSnoozeService&) =
      delete;

  // The IPH controller must call this method when the user snoozes an IPH.
  void OnUserSnooze(const base::Feature& iph_feature,
                    base::TimeDelta snooze_duration = kDefaultSnoozeDuration);

  // The IPH controller must call this method when the user actively dismiss an
  // IPH. Don't call this method in case of a passive dismiss, i.e. auto dismiss
  // after a fixed amount of time.
  void OnUserDismiss(const base::Feature& iph_feature);

  // The IPH controller must call this method after an IPH is shown.
  void OnPromoShown(const base::Feature& iph_feature);

  // The IPH controller must call this method to check if an IPH is blocked by
  // dismiss or snooze. An IPH will be approved if it is not snoozed or the
  // snoozing period has timed out.
  bool IsBlocked(const base::Feature& iph_feature);

  // Reset the state of |iph_feature|.
  virtual void Reset(const base::Feature& iph_feature) = 0;

  // Read the count of previous snoozes for |iph_feature| from profile.
  int GetSnoozeCount(const base::Feature& iph_feature);

 protected:
  // Snooze information dictionary saved under path
  // in_product_help.snoozed_feature.[iph_name] in PerfService.
  struct SnoozeData {
    bool is_dismissed = false;
    base::Time last_show_time = base::Time();
    base::Time last_snooze_time = base::Time();
    base::TimeDelta last_snooze_duration = base::TimeDelta();
    int snooze_count = 0;
    int show_count = 0;
  };

  virtual absl::optional<SnoozeData> ReadSnoozeData(
      const base::Feature& iph_feature) = 0;
  virtual void SaveSnoozeData(const base::Feature& iph_feature,
                              const SnoozeData& snooze_data) = 0;

 private:
  friend FeaturePromoSnoozeInteractiveTest;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_SNOOZE_SERVICE_H_
