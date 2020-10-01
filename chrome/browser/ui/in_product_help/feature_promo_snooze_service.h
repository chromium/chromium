// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_IN_PRODUCT_HELP_FEATURE_PROMO_SNOOZE_SERVICE_H_
#define CHROME_BROWSER_UI_IN_PRODUCT_HELP_FEATURE_PROMO_SNOOZE_SERVICE_H_

#include <string>

#include "base/optional.h"
#include "base/time/time.h"

namespace base {
struct Feature;
}  // namespace base

class Profile;
class PrefRegistrySimple;

// This service manages snooze and dismiss of in-product help promo.
//
// An IPH controller notifies this service when an IPH is snoozed or dismissed.
// When the snooze period expires, this service will approve controller's
// request to retrigger the IPH.
// If an IPH is dismissed, this service will reject controller's request.
class FeaturePromoSnoozeService {
 public:
  // Maximum count of snoozes to track in UMA histogram.
  // Snooze counts that are equal or larger than this value will be conflated.
  static const int kUmaMaxSnoozeCount;

  explicit FeaturePromoSnoozeService(Profile* profile);

  // Disallow copy and assign.
  FeaturePromoSnoozeService(const FeaturePromoSnoozeService&) = delete;
  FeaturePromoSnoozeService& operator=(const FeaturePromoSnoozeService&) =
      delete;

  // The IPH controller must call this method when the user snoozes an IPH.
  // The snooze duration defaults to 1 day plus 2 additional hours in hope to
  // stagger busy hours in the days.
  void OnUserSnooze(
      const base::Feature& iph_feature,
      base::TimeDelta snooze_duration = base::TimeDelta::FromHours(26));

  // The IPH controller must call this method when the user actively dismiss an
  // IPH. Don't call this method in case of a passive dismiss, i.e. auto dismiss
  // after a fixed amount of time.
  void OnUserDismiss(const base::Feature& iph_feature);

  // The IPH controller must call this method to check if an IPH is blocked by
  // dismiss or snooze. An IPH will be approved if it is not snoozed or the
  // snoozing period has timed out.
  bool IsBlocked(const base::Feature& iph_feature);

  // Reset the state of |iph_feature|.
  void Reset(const base::Feature& iph_feature);

  // Read the count of previous snoozes for |iph_feature| from profile.
  int GetSnoozeCount(const base::Feature& iph_feature);

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  // TODO(crbug.com/1121399): refactor prefs code so friending tests
  // isn't necessary.
  friend class FeaturePromoSnoozeInteractiveTest;

  // Snooze information dictionary saved under path
  // in_product_help.snoozed_feature.[iph_name] in PerfService.
  struct SnoozeData {
    bool is_dismissed = false;
    base::Time last_snooze_time = base::Time();
    base::TimeDelta last_snooze_duration = base::TimeDelta();
    int snooze_count = 0;
  };

  base::Optional<SnoozeData> ReadSnoozeData(const base::Feature& iph_feature);
  void SaveSnoozeData(const base::Feature& iph_feature,
                      const SnoozeData& snooze_data);

  Profile* const profile_;
};

#endif  // CHROME_BROWSER_UI_IN_PRODUCT_HELP_FEATURE_PROMO_SNOOZE_SERVICE_H_
