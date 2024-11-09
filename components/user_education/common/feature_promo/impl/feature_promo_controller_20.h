// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_FEATURE_PROMO_CONTROLLER_20_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_FEATURE_PROMO_CONTROLLER_20_H_

#include "base/memory/weak_ptr.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_registry.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/feature_promo_session_policy.h"
#include "components/user_education/common/help_bubble/help_bubble_factory_registry.h"
#include "components/user_education/common/product_messaging_controller.h"
#include "components/user_education/common/tutorial/tutorial_service.h"
#include "components/user_education/common/user_education_storage_service.h"
#include "ui/base/interaction/element_tracker.h"

namespace user_education {

// Implementation of FeaturePromoController for User Education 2.0.
class FeaturePromoController20 : public FeaturePromoControllerCommon {
 public:
  FeaturePromoController20(
      feature_engagement::Tracker* feature_engagement_tracker,
      FeaturePromoRegistry* registry,
      HelpBubbleFactoryRegistry* help_bubble_registry,
      UserEducationStorageService* storage_service,
      FeaturePromoSessionPolicy* session_policy,
      TutorialService* tutorial_service,
      ProductMessagingController* messaging_controller);

  ~FeaturePromoController20() override;

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

  // Determine if the current context and anchor element allow showing a promo.
  // This lets us rule out e.g. inactive and incognito windows/apps for
  // non-critical promos.
  //
  // Note: Implementations should make sure to check
  // `active_window_check_blocked()`.
  virtual bool CanShowPromoForElement(
      ui::TrackedElement* anchor_element) const = 0;

 private:
  struct QueuedPromoData;

  // Note: this data structure is inefficient for lookups, but given that only a
  // small number of promos should be queued at any given point, it's probably
  // still faster than some kind of linked map implementation would be.
  using QueuedPromos = std::list<QueuedPromoData>;

  // Internal entry point for showing a promo.
  FeaturePromoResult MaybeShowPromoImpl(FeaturePromoParams params,
                                        ShowSource source);

  // Registers with the ProductMessagingController if not already registered.
  void MaybeRequestMessagePriority();

  // Handles coordination with the product messaging system.
  void OnMessagePriority(RequiredNoticePriorityHandle notice_handle);

  // Handles firing async promos.
  void OnFeatureEngagementTrackerInitialized(
      bool tracker_initialized_successfully);

  // Errors out any pending queued promos.
  void FailQueuedPromos();

  // Returns the next-highest-priority queued promo, or `queued_promos_.end()`
  // if one is not present.
  QueuedPromos::iterator GetNextQueuedPromo();

  // Const version returns a pointer to the queued data, or null if no promos
  // are queued.
  const QueuedPromoData* GetNextQueuedPromo() const;

  // Returns an iterator into the queued promo list matching `iph_feature`, or
  // `queued_promos_.end()` if not found.
  QueuedPromos::iterator FindQueuedPromo(const base::Feature& iph_feature);

  // Tracks whether this controller has messaging priority.
  RequiredNoticePriorityHandle messaging_priority_handle_;

  // Tracks pending promos that have been queued (e.g. for startup).
  QueuedPromos queued_promos_;

  base::WeakPtrFactory<FeaturePromoController20> weak_ptr_factory_{this};
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_FEATURE_PROMO_CONTROLLER_20_H_
