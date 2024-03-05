// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_NEW_BADGE_CONTROLLER_H_
#define COMPONENTS_USER_EDUCATION_COMMON_NEW_BADGE_CONTROLLER_H_

#include <memory>

#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "components/user_education/common/feature_promo_data.h"
#include "components/user_education/common/feature_promo_registry.h"
#include "components/user_education/common/feature_promo_storage_service.h"
#include "components/user_education/common/new_badge_policy.h"

namespace user_education {

// Controls display of "New" Badge based on approved parameters.
class NewBadgeController {
 public:
  NewBadgeController(NewBadgeRegistry& registry,
                     FeaturePromoStorageService& storage_service,
                     std::unique_ptr<NewBadgePolicy> policy);
  NewBadgeController(const NewBadgeController&) = delete;
  void operator=(const NewBadgeController&) = delete;
  virtual ~NewBadgeController();

  // Call when a UI element that could have a "New" Badge will be shown to the
  // user. Returns true if the badge should be shown. Note that successfully
  // calling this method a number of times will permanently disable the badge,
  // so do not call this method unless the badge will actually be displayed.
  bool MaybeShowNewBadge(const base::Feature& feature);

  // Notifies that the feature associated with the badge has been shown. After
  // a certain (but low) number of uses, the badge will disappear.
  void NotifyFeatureUsed(const base::Feature& feature);

 private:
  // Checks that the `feature` is enabled and has a registered "New" Badge.
  bool CheckPrerequisites(const base::Feature& feature) const;

  const raw_ref<NewBadgeRegistry> registry_;
  const raw_ref<FeaturePromoStorageService> storage_service_;
  const std::unique_ptr<NewBadgePolicy> policy_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_NEW_BADGE_CONTROLLER_H_
