// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_FEATURE_PROMO_CONTROLLER_25_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_FEATURE_PROMO_CONTROLLER_25_H_

#include "base/memory/weak_ptr.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_registry.h"
#include "components/user_education/common/feature_promo/feature_promo_session_policy.h"
#include "components/user_education/common/help_bubble/help_bubble_factory_registry.h"
#include "components/user_education/common/product_messaging_controller.h"
#include "components/user_education/common/tutorial/tutorial_service.h"
#include "components/user_education/common/user_education_storage_service.h"

namespace user_education {

// Implementation of FeaturePromoController for User Education 2.5.
class FeaturePromoController25 : public FeaturePromoControllerCommon {
 public:
  FeaturePromoController25(
      feature_engagement::Tracker* feature_engagement_tracker,
      FeaturePromoRegistry* registry,
      HelpBubbleFactoryRegistry* help_bubble_registry,
      UserEducationStorageService* storage_service,
      FeaturePromoSessionPolicy* session_policy,
      TutorialService* tutorial_service,
      ProductMessagingController* messaging_controller);
  ~FeaturePromoController25() override;

  // FeaturePromoControllerCommon:
  void MaybeShowStartupPromo(FeaturePromoParams params) override;
  void MaybeShowPromo(FeaturePromoParams params) override;
  FeaturePromoResult MaybeShowPromoForDemoPage(
      FeaturePromoParams params) override;
  bool IsPromoQueued(const base::Feature& iph_feature) const override;

 protected:
  // FeaturePromoControllerCommon:
  FeaturePromoResult CanShowPromoCommon(
      const FeaturePromoParams& params,
      ShowSource source,
      CanShowPromoOutputs* outputs) const override;
  bool MaybeUnqueuePromo(const base::Feature& iph_feature) override;
  void MaybeShowQueuedPromo() override;
  base::WeakPtr<FeaturePromoController> GetAsWeakPtr() override;
  base::WeakPtr<FeaturePromoControllerCommon> GetCommonWeakPtr() override;

 private:
  base::WeakPtrFactory<FeaturePromoController25> weak_ptr_factory_{this};
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_FEATURE_PROMO_CONTROLLER_25_H_
