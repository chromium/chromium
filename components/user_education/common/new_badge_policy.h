// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_NEW_BADGE_POLICY_H_
#define COMPONENTS_USER_EDUCATION_COMMON_NEW_BADGE_POLICY_H_

#include "base/feature_list.h"
#include "base/time/time.h"

namespace user_education {

class FeaturePromoStorageService;
struct NewBadgeData;

// Describes when a "New" Badge can be shown for a given feature.
class NewBadgePolicy {
 public:
  NewBadgePolicy();
  NewBadgePolicy(const NewBadgePolicy&) = delete;
  void operator=(const NewBadgePolicy&) = delete;
  virtual ~NewBadgePolicy();

  // Returns whether a "New" Badge should be shown.
  virtual bool ShouldShowNewBadge(
      const NewBadgeData& data,
      const FeaturePromoStorageService& storage_service) const;

  // Records metrics for "New" Badge. Call when the badge will be shown.
  virtual void RecordNewBadgeShown(const base::Feature& feature, int count);

  // Records metrics when the associated `feature` for a "New" Badge was used.
  virtual void RecordFeatureUsed(const base::Feature& feature, int count);

  // Returns the recommended number of times to record that the feature was
  // used in prefs, which should be greater than `uses_before_dismiss` to allow
  // the limits to be adjusted up or down via Finch config. Guaranteed to be
  // significantly larger than the use cap.
  virtual int GetFeatureUsedStorageCap() const;

 protected:
  // Constructor that sets the default number of times a badge can be shown or
  // its associated entry point used before the badge disappears.
  NewBadgePolicy(int times_shown_before_dismiss,
                 int uses_before_dismiss,
                 base::TimeDelta display_window,
                 base::TimeDelta new_profile_grace_period);

 private:
  const int times_shown_before_dismiss_;
  const int uses_before_dismiss_;
  const base::TimeDelta display_window_;
  const base::TimeDelta new_profile_grace_period_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_NEW_BADGE_POLICY_H_
