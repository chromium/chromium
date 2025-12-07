// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_MESSAGING_COORDINATOR_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_MESSAGING_COORDINATOR_H_

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/user_education/common/product_messaging_controller.h"

namespace user_education::internal {

// Provides a way for a `FeaturePromoController` to coordinate with a
// `ProductMessagingController` such that the former grabs the priority handle
// when it needs to, and gets low-priority promos out of the way when important
// ones need to show.
class MessagingCoordinator {
 public:
  explicit MessagingCoordinator(ProductMessagingController& controller);
  MessagingCoordinator(const MessagingCoordinator&) = delete;
  void operator=(const MessagingCoordinator&) = delete;
  ~MessagingCoordinator();

  // Represents what kind of a promo is pending or showing.
  enum PromoState {
    kNone,
    kLowPriorityPending,
    kHighPriorityPending,
    kLowPriorityShowing,
    kHighPriorityShowing
  };

  // Returns whether a promo can be shown right now.
  bool CanShowPromo(bool high_priority) const;

  // Allows the client to update the promo state. This may result in acquiring
  // or releasing the promo handle.
  //
  // Transitioning to certain states is prohibited based on `CanShowPromo()`:
  //  - Cannot transition to a pending state if CanShowPromo is true.
  //  - Cannot transition to a showing state if CanShowPromo is false.
  //
  // This is true even if the most recent transition was already to that state.
  void TransitionToState(PromoState promo_state);

  // This callback will be fired when a promo should be preempted by some other
  // entity in the messaging service. It will not be called if the current
  // `promo_state` would not warrant preemption (so basically only when a low
  // priority promo is showing).
  base::CallbackListSubscription AddPromoPreemptedCallback(
      base::RepeatingClosure callback);

  // This callback will be fired when priority is received to show a promo.
  // It does not re-fire if priority is retained across state changes.
  base::CallbackListSubscription AddPromoReadyCallback(
      base::RepeatingClosure callback);

  // Determines whether there is another promo in line ahead of us.
  bool IsBlockedByExternalPromo() const;

  // Retrieve the current promo state. Probably only useful for testing.
  PromoState promo_state_for_testing() const { return promo_state_; }

 private:
  friend class MessagingCoordinatorTest;

  DECLARE_CLASS_REQUIRED_NOTICE_IDENTIFIER(kLowPriorityNoticeId);
  DECLARE_CLASS_REQUIRED_NOTICE_IDENTIFIER(kHighPriorityNoticeId);

  void MaybeRequestPriority(bool high_priority);
  void ReleaseAll();
  void OnPriorityReceived(RequiredNoticePriorityHandle handle);
  void OnMessageShown(RequiredNoticeId message_id);

  PromoState promo_state_ = PromoState::kNone;
  RequiredNoticePriorityHandle handle_;
  base::CallbackListSubscription message_shown_subscription_;
  base::RepeatingClosureList promo_preempted_callbacks_;
  base::RepeatingClosureList promo_ready_callbacks_;
  raw_ref<ProductMessagingController> controller_;
  base::WeakPtrFactory<MessagingCoordinator> weak_ptr_factory_{this};
};

}  // namespace user_education::internal

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_MESSAGING_COORDINATOR_H_
