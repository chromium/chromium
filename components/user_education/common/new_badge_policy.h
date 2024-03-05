// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_NEW_BADGE_POLICY_H_
#define COMPONENTS_USER_EDUCATION_COMMON_NEW_BADGE_POLICY_H_

#include "base/feature_list.h"
#include "components/user_education/common/feature_promo_data.h"

namespace user_education {

// Describes when a "New" Badge can be shown for a given feature.
class NewBadgePolicy {
 public:
  NewBadgePolicy();
  NewBadgePolicy(const NewBadgePolicy&) = delete;
  void operator=(const NewBadgePolicy&) = delete;
  virtual ~NewBadgePolicy();

  // Returns whether a "New" Badge for `feature` should be shown.
  virtual bool ShouldShowNewBadge(const base::Feature& feature,
                                  int show_count,
                                  int used_count,
                                  base::TimeDelta time_since_enabled) const;

 protected:
  // Constructor that sets the default number of times a badge can be shown or
  // its associated entry point used before the badge disappears.
  NewBadgePolicy(int times_shown_before_dismiss,
                 int uses_before_dismiss,
                 base::TimeDelta display_window);

 private:
  const int times_shown_before_dismiss_;
  const int uses_before_dismiss_;
  const base::TimeDelta display_window_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_NEW_BADGE_POLICY_H_
