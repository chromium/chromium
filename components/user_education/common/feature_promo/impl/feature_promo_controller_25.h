// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_FEATURE_PROMO_CONTROLLER_25_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_FEATURE_PROMO_CONTROLLER_25_H_

#include <memory>

#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_lifecycle.h"
#include "components/user_education/common/feature_promo/feature_promo_registry.h"
#include "components/user_education/common/feature_promo/feature_promo_session_policy.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/common/feature_promo/impl/precondition_list_provider.h"
#include "components/user_education/common/help_bubble/help_bubble_factory_registry.h"
#include "components/user_education/common/product_messaging_controller.h"
#include "components/user_education/common/tutorial/tutorial_service.h"
#include "components/user_education/common/user_education_storage_service.h"

namespace user_education {

// Implementation of FeaturePromoController for User Education 2.5.
class FeaturePromoController25 : public FeaturePromoControllerCommon {
 public:
  // Use the same priority rankings as the rest of User Education.
  using Priority = FeaturePromoPriorityProvider::PromoPriority;
  using PromoWeight = FeaturePromoPriorityProvider::PromoWeight;

  // Delay between checking to see if promos can show.
  static constexpr base::TimeDelta kPollDelay = base::Milliseconds(500);

  FeaturePromoController25(
      feature_engagement::Tracker* feature_engagement_tracker,
      FeaturePromoRegistry* registry,
      HelpBubbleFactoryRegistry* help_bubble_registry,
      UserEducationStorageService* storage_service,
      FeaturePromoSessionPolicy* session_policy,
      TutorialService* tutorial_service,
      ProductMessagingController* messaging_controller);
  ~FeaturePromoController25() override;

  // Perform required initialization that cannot be safely done in the
  // constructor. Derived classes MUST call the base class version of this
  // method.
  virtual void Init();

  // FeaturePromoControllerCommon:
  FeaturePromoResult CanShowPromo(
      const FeaturePromoParams& params) const override;
  void MaybeShowStartupPromo(FeaturePromoParams params) override;
  void MaybeShowPromo(FeaturePromoParams params) override;
  void MaybeShowPromoForDemoPage(FeaturePromoParams params) override;
  bool IsPromoQueued(const base::Feature& iph_feature) const override;

 protected:
  // FeaturePromoControllerCommon:
  bool MaybeUnqueuePromo(const base::Feature& iph_feature) override;
  void MaybeShowQueuedPromo() override;
  base::WeakPtr<FeaturePromoController> GetAsWeakPtr() override;
  base::WeakPtr<FeaturePromoControllerCommon> GetCommonWeakPtr() override;

  // This needs to be called by derived class destructor to ensure proper
  // order of cleanup.
  void OnDestroying();

  virtual void AddDemoPreconditionProviders(
      ComposingPreconditionListProvider& to_add_to,
      bool required);
  virtual void AddPreconditionProviders(
      ComposingPreconditionListProvider& to_add_to,
      Priority priority,
      bool required);

 private:
  struct PromoData;
  struct PrivateData;

  // Returns whether there's a demo promo in the queue.
  bool IsDemoPending() const;

  // Returns whether there are any promos queued in the non-demo queues.
  bool IsPromoQueued() const;

  // Computes and returns the promo data based on the current state of the
  // controller - i.e. what is queued, if any promos are eligible, what handles
  // are held, etc. Will update/clean queues and pop an eligible promo; will not
  // touch any other internal state.
  PromoData GetNextPromoData();

  // Shows the promo specified in `promo_data`, which must have valid params.
  FeaturePromoResult ShowPromo(PromoData& promo_data);

  // Posts an update to the queues. Doesn't do anything if an update is already
  // imminently pending.
  void MaybePostUpdate();

  // Updates all of the internal state and sees if there is a promo eligible to
  // run. May stop or start the poll timer. May request or dispose a product
  // messaging handle. May show a promo. This is the primary entry point for all
  // state updates.
  void UpdateQueuesAndMaybeShowPromo();

  // Always poll when anything is in queue. This prevents a long-running
  // promotion from preventing other promos from timing out.
  void UpdatePollingState();

  // Called when waiting for messaging priority, when it is actually granted.
  void OnMessagingPriority();

  // Called when the current promo is preempted by a higher-priority product
  // message.
  void OnPromoPreempted();

  bool update_pending_ = false;
  std::unique_ptr<PrivateData> private_;
  base::RepeatingTimer poller_;
  const std::string demo_feature_name_;
  const raw_ref<ProductMessagingController> product_messaging_controller_;
  base::WeakPtrFactory<FeaturePromoController25> weak_ptr_factory_{this};
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_FEATURE_PROMO_CONTROLLER_25_H_
