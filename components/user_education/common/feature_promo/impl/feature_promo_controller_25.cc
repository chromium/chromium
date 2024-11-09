// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo/impl/feature_promo_controller_25.h"

#include "base/notreached.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"

namespace user_education {

FeaturePromoController25::FeaturePromoController25(
    feature_engagement::Tracker* feature_engagement_tracker,
    FeaturePromoRegistry* registry,
    HelpBubbleFactoryRegistry* help_bubble_registry,
    UserEducationStorageService* storage_service,
    FeaturePromoSessionPolicy* session_policy,
    TutorialService* tutorial_service,
    ProductMessagingController* messaging_controller)
    : FeaturePromoControllerCommon(feature_engagement_tracker,
                                   registry,
                                   help_bubble_registry,
                                   storage_service,
                                   session_policy,
                                   tutorial_service,
                                   messaging_controller) {}

FeaturePromoController25::~FeaturePromoController25() = default;

void FeaturePromoController25::MaybeShowStartupPromo(
    FeaturePromoParams params) {}

void FeaturePromoController25::MaybeShowPromo(FeaturePromoParams params) {
  MaybeShowStartupPromo(std::move(params));
}

FeaturePromoResult FeaturePromoController25::MaybeShowPromoForDemoPage(
    FeaturePromoParams params) {
  return FeaturePromoResult::kError;
}

bool FeaturePromoController25::IsPromoQueued(
    const base::Feature& iph_feature) const {
  return false;
}

FeaturePromoResult FeaturePromoController25::CanShowPromoCommon(
    const FeaturePromoParams& params,
    ShowSource source,
    CanShowPromoOutputs* outputs) const {
  return FeaturePromoResult::kError;
}

bool FeaturePromoController25::MaybeUnqueuePromo(
    const base::Feature& iph_feature) {
  return false;
}

void FeaturePromoController25::MaybeShowQueuedPromo() {
  NOTREACHED();
}

base::WeakPtr<FeaturePromoController> FeaturePromoController25::GetAsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

base::WeakPtr<FeaturePromoControllerCommon>
FeaturePromoController25::GetCommonWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace user_education
