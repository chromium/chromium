// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_NEW_BADGE_SPECIFICATION_H_
#define COMPONENTS_USER_EDUCATION_COMMON_NEW_BADGE_SPECIFICATION_H_

#include "base/feature_list.h"
#include "components/user_education/common/user_education_metadata.h"

namespace user_education {

// Describes a "New" Badge that will be displayed on a menu item or other UI
// element.
//
// The badge will be displayed by default when the feature is enabled, up to a
// minimum number of views by the user, or a smaller number of uses of the
// associated feature.
//
// The badge must be removed when the associated feature rolls out to 100% and
// the Feature's default state changes to `FEATURE_ENABLED_BY_DEFAULT`.
//
// To A/B test the badge independently of enabling the feature, add the optional
// parameter "show_new_badge: false" to the feature config in the arm of the
// Finch study where you want to suppress the badge.
struct NewBadgeSpecification {
  NewBadgeSpecification();
  NewBadgeSpecification(const base::Feature& feature, Metadata metadata);
  NewBadgeSpecification(NewBadgeSpecification&&) noexcept;
  NewBadgeSpecification& operator=(NewBadgeSpecification&&) noexcept;
  ~NewBadgeSpecification();

  // The feature the badge will be tied to.
  raw_ptr<const base::Feature> feature = nullptr;

  // Additional information about the badge, including ownership, release
  // milestone, feature dependencies, and general description.
  //
  // This data is used to display information on the User Education Internals
  // Page.
  Metadata metadata;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_NEW_BADGE_SPECIFICATION_H_
