// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_NEW_BADGE_CONTROLLER_H_
#define COMPONENTS_USER_EDUCATION_COMMON_NEW_BADGE_CONTROLLER_H_

#include <memory>

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "components/user_education/common/feature_promo_data.h"
#include "components/user_education/common/feature_promo_registry.h"
#include "components/user_education/common/feature_promo_storage_service.h"
#include "components/user_education/common/new_badge_policy.h"
#include "ui/base/models/simple_menu_model.h"

namespace user_education {

using DisplayNewBadge = ui::IsNewFeatureAtValue;

// Controls display of "New" Badge based on approved parameters.
class NewBadgeController {
 public:
  NewBadgeController(NewBadgeRegistry& registry,
                     FeaturePromoStorageService& storage_service,
                     std::unique_ptr<NewBadgePolicy> policy);
  NewBadgeController(const NewBadgeController&) = delete;
  void operator=(const NewBadgeController&) = delete;
  virtual ~NewBadgeController();

  // Called after registration of "New" Badges to ensure that all data is
  // consistent.
  void InitData();

  // Call when a UI element that could have a "New" Badge will be shown to the
  // user. Returns true if the badge should be shown. Note that successfully
  // calling this method a number of times will permanently disable the badge,
  // so do not call this method unless the badge will actually be displayed.
  DisplayNewBadge MaybeShowNewBadge(const base::Feature& feature);

  // Notifies that the `feature` associated with the badge has been shown. After
  // a certain (but low) number of uses, the badge will disappear. Fails if
  // there is no new badge registered for this feature.
  void NotifyFeatureUsed(const base::Feature& feature);

  // As NotifyFeatureUsed, but if there is no new badge registered for the given
  // feature or it is not enabled, does not generate an error.
  void NotifyFeatureUsedIfValid(const base::Feature& feature);

  // Disables "New" Badges for tests - specifically pixel tests, where the
  // presence of a badge could disrupt the expected image.
  //
  // Be sure to store the resulting lock and then release it at the end of the
  // test; badges are only disabled while the returned object is alive.
  using TestLock = std::unique_ptr<base::AutoReset<bool>>;
  [[nodiscard]] static TestLock DisableNewBadgesForTesting();

 private:
  void NotifyFeatureUsedImpl(const base::Feature& feature,
                             bool allow_not_registered);

  // Checks that the `feature` is enabled and has a registered "New" Badge.
  // If the feature is not registered and `allow_not_registered` is false, will
  // generate an error.
  bool CheckPrerequisites(const base::Feature& feature,
                          bool allow_not_registered) const;

  const raw_ref<NewBadgeRegistry> registry_;
  const raw_ref<FeaturePromoStorageService> storage_service_;
  const std::unique_ptr<NewBadgePolicy> policy_;
  static bool disable_new_badges_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_NEW_BADGE_CONTROLLER_H_
