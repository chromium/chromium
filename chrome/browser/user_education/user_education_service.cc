// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_education/user_education_service.h"

#include <memory>

#include "base/feature_list.h"
#include "components/user_education/common/feature_promo_session_policy.h"
#include "components/user_education/common/feature_promo_storage_service.h"
#include "components/user_education/common/user_education_features.h"

const char kSidePanelCustomizeChromeTutorialId[] =
    "Side Panel Customize Chrome Tutorial";
const char kTabGroupTutorialId[] = "Tab Group Tutorial";
const char kPasswordManagerTutorialId[] = "Password Manager Tutorial";

UserEducationService::UserEducationService(
    std::unique_ptr<user_education::FeaturePromoStorageService> storage_service)
    : tutorial_service_(&tutorial_registry_, &help_bubble_factory_registry_),
      feature_promo_storage_service_(std::move(storage_service)),
      feature_promo_session_policy_(
          user_education::features::IsUserEducationV2()
              ? std::make_unique<user_education::FeaturePromoSessionPolicyV2>()
              : std::make_unique<user_education::FeaturePromoSessionPolicy>()) {
  feature_promo_session_policy_->Init(&feature_promo_session_manager_,
                                      feature_promo_storage_service_.get());
}

UserEducationService::~UserEducationService() = default;
