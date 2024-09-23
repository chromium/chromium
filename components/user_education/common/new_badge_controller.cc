// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/new_badge_controller.h"

#include "components/user_education/common/feature_promo_data.h"
#include "components/user_education/common/feature_promo_storage_service.h"
#include "components/user_education/common/user_education_features.h"
#include "ui/base/models/simple_menu_model.h"

namespace user_education {

// static
bool NewBadgeController::disable_new_badges_ = false;

NewBadgeController::NewBadgeController(
    NewBadgeRegistry& registry,
    FeaturePromoStorageService& storage_service,
    std::unique_ptr<NewBadgePolicy> policy)
    : registry_(registry),
      storage_service_(storage_service),
      policy_(std::move(policy)) {}

void NewBadgeController::InitData() {
  // Ensure that all registered New Badge features that are enabled have their
  // `feature_enabled_time` set.
  for (const auto& [feature, spec] : registry_->feature_data()) {
    if (base::FeatureList::IsEnabled(*feature)) {
      auto data = storage_service_->ReadNewBadgeData(*feature);
      if (data.feature_enabled_time.is_null()) {
        data.feature_enabled_time = base::Time::Now();
        storage_service_->SaveNewBadgeData(*feature, data);
      }
    }
  }
}

NewBadgeController::~NewBadgeController() = default;

DisplayNewBadge NewBadgeController::MaybeShowNewBadge(
    const base::Feature& feature) {
  if (disable_new_badges_) {
    return DisplayNewBadge();
  }

  if (!CheckPrerequisites(feature, /*allow_not_registered=*/false)) {
    return DisplayNewBadge();
  }

  NewBadgeData data = storage_service_->ReadNewBadgeData(feature);
  CHECK(!data.feature_enabled_time.is_null())
      << "Feature enabled time for " << feature.name
      << " is null; this value should have been set during initialization.";

  // Check the policy.
  if (!policy_->ShouldShowNewBadge(data, *storage_service_)) {
    return ui::IsNewFeatureAtValue();
  }

  ++data.show_count;
  storage_service_->SaveNewBadgeData(feature, data);
  policy_->RecordNewBadgeShown(feature, data.show_count);
  return DisplayNewBadge(base::PassKey<NewBadgeController>(), true);
}

void NewBadgeController::NotifyFeatureUsed(const base::Feature& feature) {
  NotifyFeatureUsedImpl(feature, /*allow_not_registered=*/false);
}

void NewBadgeController::NotifyFeatureUsedIfValid(
    const base::Feature& feature) {
  NotifyFeatureUsedImpl(feature, /*allow_not_registered=*/true);
}

void NewBadgeController::NotifyFeatureUsedImpl(const base::Feature& feature,
                                               bool allow_not_registered) {
  if (!CheckPrerequisites(feature, allow_not_registered)) {
    return;
  }

  NewBadgeData data = storage_service_->ReadNewBadgeData(feature);

  // Maybe record histograms.
  policy_->RecordFeatureUsed(feature, ++data.used_count);

  // Maybe save the updated data.
  const int cap = policy_->GetFeatureUsedStorageCap();
  if (data.used_count <= cap) {
    storage_service_->SaveNewBadgeData(feature, data);
  }
}

bool NewBadgeController::CheckPrerequisites(const base::Feature& feature,
                                            bool allow_not_registered) const {
  // It's possible the same entry point is being re-used for the new feature as
  // for an older version; just ignore cases where the new feature is not
  // enabled.
  if (!base::FeatureList::IsEnabled(feature)) {
    return false;
  }

  // While the registry is largely present to provide metadata, it's not
  // possible to correctly calculate the display window if the feature isn't
  // registered. Therefore "New" Badges that are not registered are not allowed.
  if (!registry_->GetParamsForFeature(feature)) {
    DUMP_WILL_BE_CHECK(allow_not_registered)
        << "Tried to access \"New\" Badge data for feature " << feature.name
        << " but new badge was never registered! See "
           "browser_user_education_service.cc for full list of registered "
           "badges.";
    return false;
  }

  return true;
}

// static
NewBadgeController::TestLock NewBadgeController::DisableNewBadgesForTesting() {
  return std::make_unique<base::AutoReset<bool>>(&disable_new_badges_, true);
}

}  // namespace user_education
