// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_COMMON_PRECONDITIONS_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_COMMON_PRECONDITIONS_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/user_education/common/anchor_element_provider.h"
#include "components/user_education/common/feature_promo/feature_promo_lifecycle.h"
#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/feature_promo/feature_promo_session_policy.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/typed_identifier.h"

namespace user_education {

DECLARE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(
    kFeatureEngagementTrackerInitializedPrecondition);
DECLARE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(
    kMeetsFeatureEngagementCriteriaPrecondition);
DECLARE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kAnchorElementPrecondition);
DECLARE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kLifecyclePrecondition);
DECLARE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kSessionPolicyPrecondition);

// Represents a precondition requiring the Feature Engagement Tracker to be
// initialized.
class FeatureEngagementTrackerInitializedPrecondition
    : public CachingFeaturePromoPrecondition {
 public:
  // Specifies the tracker to be used. If the tracker is null or the tracker
  // fails to initialize, the `failure()` for this precondition becomes
  // `kError`.
  explicit FeatureEngagementTrackerInitializedPrecondition(
      feature_engagement::Tracker* tracker);
  ~FeatureEngagementTrackerInitializedPrecondition() override;

 private:
  void OnFeatureEngagementTrackerInitialized(
      bool tracker_initialized_successfully);

  base::WeakPtrFactory<FeatureEngagementTrackerInitializedPrecondition>
      weak_ptr_factory_{this};
};

// Represents the requirement that a feature is not excluded by the Feature
// Engagement Tracker based on event counts.
class MeetsFeatureEngagementCriteriaPrecondition
    : public FeaturePromoPreconditionBase {
 public:
  MeetsFeatureEngagementCriteriaPrecondition(
      const base::Feature& feature,
      const feature_engagement::Tracker& tracker);
  ~MeetsFeatureEngagementCriteriaPrecondition() override;

  // FeaturePromoPrecondition:
  FeaturePromoResult CheckPrecondition() const override;

 private:
  const raw_ref<const base::Feature> feature_;
  const raw_ref<const feature_engagement::Tracker> tracker_;
};

// Represents the requirement that an anchor element is present and visible.
class AnchorElementPrecondition : public FeaturePromoPreconditionBase {
 public:
  DECLARE_CLASS_TYPED_IDENTIFIER_VALUE(ui::SafeElementReference,
                                       kAnchorElement);

  AnchorElementPrecondition(const AnchorElementProvider& provider,
                            ui::ElementContext default_context);
  ~AnchorElementPrecondition() override;

  // FeaturePromoPrecondition:
  FeaturePromoResult CheckPrecondition() const override;

 private:
  const raw_ref<const AnchorElementProvider> provider_;
  const ui::ElementContext default_context_;
};

// Wraps a FeaturePromoLifecycle to determine if a promo can be shown.
class LifecyclePrecondition : public FeaturePromoPreconditionBase {
 public:
  DECLARE_CLASS_TYPED_IDENTIFIER_VALUE(std::unique_ptr<FeaturePromoLifecycle>,
                                       kLifecycle);
  LifecyclePrecondition(std::unique_ptr<FeaturePromoLifecycle>, bool for_demo);
  ~LifecyclePrecondition() override;

  // FeaturePromoPrecondition:
  FeaturePromoResult CheckPrecondition() const override;

 private:
  const bool for_demo_;
};

// Wraps a FeaturePromoSessionPolicy to determine if a promo can be shown.
class SessionPolicyPrecondition : public FeaturePromoPreconditionBase {
 public:
  using GetCurrentPromoInfoCallback = base::RepeatingCallback<
      std::optional<FeaturePromoPriorityProvider::PromoPriorityInfo>()>;

  SessionPolicyPrecondition(
      FeaturePromoSessionPolicy* session_policy,
      FeaturePromoPriorityProvider::PromoPriorityInfo priority_info,
      GetCurrentPromoInfoCallback get_current_promo_info_callback);
  ~SessionPolicyPrecondition() override;

  // FeaturePromoPrecondition:
  FeaturePromoResult CheckPrecondition() const override;

 private:
  const raw_ref<FeaturePromoSessionPolicy> session_policy_;
  const FeaturePromoPriorityProvider::PromoPriorityInfo priority_info_;
  const GetCurrentPromoInfoCallback get_current_promo_info_callback_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_COMMON_PRECONDITIONS_H_
