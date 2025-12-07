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
#include "components/user_education/common/user_education_context.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/typed_data_collection.h"
#include "ui/base/interaction/typed_identifier.h"

namespace user_education {

DECLARE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(
    kFeatureEnabledPrecondition);
DECLARE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(
    kFeatureEngagementTrackerInitializedPrecondition);
DECLARE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(
    kMeetsFeatureEngagementCriteriaPrecondition);
DECLARE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kContextValidPrecondition);
DECLARE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kAnchorElementPrecondition);
DECLARE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kLifecyclePrecondition);
DECLARE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kSessionPolicyPrecondition);

// Represents a precondition requiring a promo's feature to be enabled.
class FeatureEnabledPrecondition : public CachingFeaturePromoPrecondition {
 public:
  explicit FeatureEnabledPrecondition(const base::Feature& iph_feature);
  ~FeatureEnabledPrecondition() override;
};

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
//
// For example, if the FET config has:
//   "button_clicked_event": LESS_THAN_OR_EQUAL, 3
// ...then if "button_clicked_event" has happened 4 times this precondition
// will fail, but if it has happened three or fewer times it will succeed.
class MeetsFeatureEngagementCriteriaPrecondition
    : public FeaturePromoPreconditionBase {
 public:
  MeetsFeatureEngagementCriteriaPrecondition(
      const base::Feature& feature,
      const feature_engagement::Tracker& tracker);
  ~MeetsFeatureEngagementCriteriaPrecondition() override;

  // FeaturePromoPrecondition:
  FeaturePromoResult CheckPrecondition(
      ui::UnownedTypedDataCollection& data) const override;

 private:
  const raw_ref<const base::Feature> feature_;
  const raw_ref<const feature_engagement::Tracker> tracker_;
};

// Requires the context for the promo to still be valid. Should be evaluated
// before AnchorElementPrecondition.
class ContextValidPrecondition : public FeaturePromoPreconditionBase {
 public:
  explicit ContextValidPrecondition(const UserEducationContextPtr& context);
  ~ContextValidPrecondition() override;

  // FeaturePromoPrecondition:
  FeaturePromoResult CheckPrecondition(
      ui::UnownedTypedDataCollection& data) const override;

 private:
  const UserEducationContextPtr context_;
};

// Represents the requirement that an anchor element is present and visible.
class AnchorElementPrecondition : public FeaturePromoPreconditionBase {
 public:
  DECLARE_CLASS_TYPED_IDENTIFIER_VALUE(std::optional<int>, kRotatingPromoIndex);
  DECLARE_CLASS_TYPED_IDENTIFIER_VALUE(ui::SafeElementReference,
                                       kAnchorElement);

  AnchorElementPrecondition(const AnchorElementProvider& provider,
                            ui::ElementContext default_context,
                            bool pre_increment_index);
  ~AnchorElementPrecondition() override;

  // FeaturePromoPrecondition:
  FeaturePromoResult CheckPrecondition(
      ui::UnownedTypedDataCollection& data) const override;

 private:
  const raw_ref<const AnchorElementProvider> provider_;
  const ui::ElementContext default_context_;

  // Whether to increment the rotating promo index before computing.
  // Used for demos in certain cases; see constructor call site.
  const bool pre_increment_index_;
};

// Wraps a FeaturePromoLifecycle to determine if a promo can be shown.
class LifecyclePrecondition : public FeaturePromoPreconditionBase {
 public:
  DECLARE_CLASS_TYPED_IDENTIFIER_VALUE(std::unique_ptr<FeaturePromoLifecycle>,
                                       kLifecycle);
  LifecyclePrecondition(std::unique_ptr<FeaturePromoLifecycle>, bool for_demo);
  ~LifecyclePrecondition() override;

  // FeaturePromoPrecondition:
  FeaturePromoResult CheckPrecondition(
      ui::UnownedTypedDataCollection& data) const override;

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
  FeaturePromoResult CheckPrecondition(
      ui::UnownedTypedDataCollection& data) const override;

 private:
  const raw_ref<FeaturePromoSessionPolicy> session_policy_;
  const FeaturePromoPriorityProvider::PromoPriorityInfo priority_info_;
  const GetCurrentPromoInfoCallback get_current_promo_info_callback_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_COMMON_PRECONDITIONS_H_
