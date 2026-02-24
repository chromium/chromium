// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_FEATURE_PROMO_CONTROLLER_IMPL_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_FEATURE_PROMO_CONTROLLER_IMPL_H_

#include <memory>
#include <set>

#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_handle.h"
#include "components/user_education/common/feature_promo/feature_promo_lifecycle.h"
#include "components/user_education/common/feature_promo/feature_promo_registry.h"
#include "components/user_education/common/feature_promo/feature_promo_session_policy.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/common/feature_promo/impl/precondition_list_provider.h"
#include "components/user_education/common/help_bubble/help_bubble_factory_registry.h"
#include "components/user_education/common/product_messaging_controller.h"
#include "components/user_education/common/tutorial/tutorial_service.h"
#include "components/user_education/common/user_education_storage_service.h"

// Declaring these in the global namespace for testing purposes.
class BrowserFeaturePromoControllerTestBase;
class BrowserFeaturePromoControllerTestHelper;
class FeaturePromoLifecycleUiTest;

namespace user_education {

// Manages display of in-product help promos. All IPH displays in Top
// Chrome should go through here.
class FeaturePromoControllerImpl : public FeaturePromoController {
 public:
  // Use the same priority rankings as the rest of User Education.
  using Priority = FeaturePromoPriorityProvider::PromoPriority;
  using PromoWeight = FeaturePromoPriorityProvider::PromoWeight;

  using TestLock = std::unique_ptr<base::AutoReset<bool>>;

  // Delay between checking to see if promos can show.
  static constexpr base::TimeDelta kPollDelay = base::Milliseconds(500);

  FeaturePromoControllerImpl(
      feature_engagement::Tracker* feature_engagement_tracker,
      FeaturePromoRegistry* registry,
      HelpBubbleFactoryRegistry* help_bubble_registry,
      UserEducationStorageService* storage_service,
      FeaturePromoSessionPolicy* session_policy,
      TutorialService* tutorial_service,
      ProductMessagingController* messaging_controller);
  ~FeaturePromoControllerImpl() override;

  // Perform required initialization that cannot be safely done in the
  // constructor. Derived classes MUST call the base class version of this
  // method.
  virtual void Init();

  // FeaturePromoController:
  FeaturePromoResult CanShowPromo(
      const FeaturePromoParams& params,
      const UserEducationContextPtr& context) const override;
  void MaybeShowStartupPromo(FeaturePromoParams params,
                             UserEducationContextPtr context) override;
  void MaybeShowPromo(FeaturePromoParams params,
                      UserEducationContextPtr context) override;
  void MaybeShowPromoForDemoPage(FeaturePromoParams params,
                                 UserEducationContextPtr context) override;

  // Required to use one-argument version.
  using FeaturePromoController::HasPromoBeenDismissed;

  // FeaturePromoController:
  FeaturePromoStatus GetPromoStatus(
      const base::Feature& iph_feature) const override;
  const FeaturePromoSpecification* GetCurrentPromoSpecificationForAnchor(
      ui::ElementIdentifier menu_element_id) const override;
  bool HasPromoBeenDismissed(
      const FeaturePromoParams& params,
      FeaturePromoClosedReason* close_reason) const override;
  bool DismissNonCriticalBubbleInRegion(
      const gfx::Rect& screen_bounds) override;
  bool EndPromo(const base::Feature& iph_feature,
                EndFeaturePromoReason end_promo_reason) override;
  FeaturePromoHandle CloseBubbleAndContinuePromo(
      const base::Feature& iph_feature) final;
#if !BUILDFLAG(IS_ANDROID)
  void NotifyFeatureUsedIfValid(const base::Feature& feature) override;
#endif
  base::WeakPtr<FeaturePromoController> GetAsWeakPtr() override;

  // Returns whether `iph_feature` is queued to be shown.
  bool IsPromoQueued(const base::Feature& iph_feature) const;

  // Removes a promo from the queue and returns whether the promo was found and
  // canceled.
  virtual bool MaybeUnqueuePromo(const base::Feature& iph_feature);

  // Possibly fires a queued promo based on certain conditions.
  virtual void MaybeShowQueuedPromo();

  // Gets a typed weak pointer to this object.
  base::WeakPtr<FeaturePromoControllerImpl> GetImplWeakPtr();

  const HelpBubbleFactoryRegistry* bubble_factory_registry() const {
    return bubble_factory_registry_;
  }
  HelpBubbleFactoryRegistry* bubble_factory_registry() {
    return bubble_factory_registry_;
  }

  HelpBubble* promo_bubble_for_testing() { return promo_bubble(); }

  TutorialService* tutorial_service_for_testing() { return tutorial_service_; }

  // Blocks a check whether the IPH would be created in an inactive window or
  // app before showing the IPH.
  //
  // Intended for unit tests. For browser and interactive tests, prefer to use
  // `InteractiveFeaturePromoTest`.
  [[nodiscard]] static TestLock BlockActiveWindowCheckForTesting();

  // Returns true if `BlockActiveWindowCheckForTesting()` is active.
  static bool active_window_check_blocked() {
    return active_window_check_blocked_;
  }

 protected:
  enum class ShowSource { kNormal, kQueue, kDemo };

  // Records when and why an IPH was not shown.
  void RecordPromoNotShown(const char* feature_name,
                           FeaturePromoResult::Failure failure) const final;

  const base::Feature* GetCurrentPromoFeature() const final;

  // Method that creates the bubble for a feature promo. May return null if the
  // bubble cannot be shown.
  std::unique_ptr<HelpBubble> ShowPromoBubbleImpl(
      FeaturePromoSpecification::BuildHelpBubbleParams build_params,
      UserEducationContextPtr context);

  // Does the work of ending a promo with the specified `close_reason`.
  bool EndPromo(const base::Feature& iph_feature,
                FeaturePromoClosedReason close_reason);

  // Closes any existing help bubble in `context`; usually called after
  // canceling any existing promo to clear up tutorial bubbles, etc.
  void CloseHelpBubbleIfPresent(ui::ElementContext context);

  // Returns whether we can play a screen reader prompt for the "focus help
  // bubble" promo.
  // TODO(crbug.com/40200981): This must be called *before* we ask if the bubble
  // will show because a limitation in the current FE backend causes
  // ShouldTriggerHelpUI() to always return false if another promo is being
  // displayed. Once we have machinery to allow concurrency in the FE system
  // all of this logic can be rewritten.
  bool CheckExtendedPropertiesPromptAvailable(bool for_demo) const;

  // Creates a lifecycle for the given promo.
  std::unique_ptr<FeaturePromoLifecycle> CreateLifecycleFor(
      const FeaturePromoSpecification& spec,
      const FeaturePromoParams& params) const;

  // Derived classes need non-const access to these members in const methods.
  // Be careful when calling them.
  UserEducationStorageService* storage_service() const {
    return storage_service_;
  }
  feature_engagement::Tracker* feature_engagement_tracker() const {
    return feature_engagement_tracker_;
  }
  FeaturePromoSessionPolicy* session_policy() { return session_policy_; }
  const FeaturePromoSessionPolicy* session_policy() const {
    return session_policy_;
  }

  FeaturePromoLifecycle* current_promo() { return current_promo_.get(); }
  const FeaturePromoLifecycle* current_promo() const {
    return current_promo_.get();
  }
  void set_current_promo(std::unique_ptr<FeaturePromoLifecycle> current_promo) {
    current_promo_ = std::move(current_promo);
  }
  const FeaturePromoPriorityProvider::PromoPriorityInfo& last_promo_info()
      const {
    return last_promo_info_;
  }
  void set_last_promo_info(
      const FeaturePromoPriorityProvider::PromoPriorityInfo& last_promo_info) {
    last_promo_info_ = last_promo_info;
  }
  HelpBubble* promo_bubble() {
    return current_promo_ ? current_promo_->help_bubble() : nullptr;
  }
  const HelpBubble* promo_bubble() const {
    return current_promo_ ? current_promo_->help_bubble() : nullptr;
  }

  // Saves the close callback for the current bubble.
  void set_bubble_closed_callback(BubbleCloseCallback callback) {
    bubble_closed_callback_ = std::move(callback);
  }

  const FeaturePromoRegistry* registry() const { return registry_; }
  FeaturePromoRegistry* registry() { return registry_; }

  // Gets the alt text to use for body icons.
  virtual std::u16string GetBodyIconAltText() const = 0;

  // Gets the feature associated with prompting the user how to navigate to help
  // bubbles via the keyboard. It is its own promo, and will stop playing in
  // most cases when the user has made use of it enough times.
  //
  // If null is returned, no attempt will be made to play a prompt.
  virtual const base::Feature* GetScreenReaderPromptPromoFeature() const = 0;

  // This is the associated event with the promo feature above. The event is
  // recorded only if and when the promo is actually played to the user.
  virtual const char* GetScreenReaderPromptPromoEventName() const = 0;

  // Returns the special prompt to play with the initial bubble of a tutorial;
  // instead of the general navigation help prompt returned by
  // GetFocusHelpBubbleScreenReaderHint().
  virtual std::u16string GetTutorialScreenReaderHint(
      const ui::AcceleratorProvider* accelerator_provider) const = 0;

  // This method returns an appropriate prompt for promoting using a navigation
  // accelerator to focus the help bubble.
  virtual std::u16string GetFocusHelpBubbleScreenReaderHint(
      FeaturePromoSpecification::PromoType promo_type,
      ui::TrackedElement* anchor_element,
      const ui::AcceleratorProvider* accelerator_provider) const = 0;

  // Returns the anchor context for a help bubble, in case the help bubble isn't
  // in the same context as the caller. May return null.
  virtual UserEducationContextPtr GetContextForHelpBubble(
      const ui::TrackedElement* anchor_element) const = 0;

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
  friend BrowserFeaturePromoControllerTestBase;
  friend BrowserFeaturePromoControllerTestHelper;
  friend FeaturePromoLifecycleUiTest;

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

  void RecordPromoEnded(FeaturePromoClosedReason close_reason,
                        bool continue_after_close);

  FeaturePromoHandle CloseBubbleAndContinuePromoWithReason(
      const base::Feature& iph_action,
      FeaturePromoClosedReason close_reason);

  // FeaturePromoController:
  void FinishContinuedPromo(const base::Feature& iph_feature) override;

  // Callback that cleans up a help bubble when it is closed.
  void OnHelpBubbleClosed(HelpBubble* bubble, HelpBubble::CloseReason reason);

  // Callback when the help bubble times out.
  void OnHelpBubbleTimedOut(const base::Feature* feature);

  // Callback for snoozed features.
  void OnHelpBubbleSnoozed(const base::Feature* feature);

  // Callback for snoozed tutorial features. .
  void OnTutorialHelpBubbleSnoozed(const base::Feature* iph_feature,
                                   TutorialIdentifier tutorial_id);

  // Callback when a feature's help bubble times out.
  void OnHelpBubbleTimeout(const base::Feature* feature);

  // Callback when a feature's help bubble is dismissed by any means other than
  // snoozing (including "OK" or "Got it!" buttons).
  void OnHelpBubbleDismissed(const base::Feature* feature,
                             bool via_action_button);

  // Callback when the dismiss button for IPH for tutorials is clicked.
  void OnTutorialHelpBubbleDismissed(const base::Feature* iph_feature,
                                     TutorialIdentifier tutorial_id);

  // Callback when a tutorial triggered from a promo is actually started.
  void OnTutorialStarted(const base::Feature* iph_feature,
                         const UserEducationContextPtr& context,
                         const UserEducationContextPtr& bubble_context,
                         TutorialIdentifier tutorial_id);

  // Called when a tutorial launched via StartTutorial() completes.
  void OnTutorialComplete(const base::Feature* iph_feature);

  // Called when a tutorial launched via StartTutorial() aborts.
  void OnTutorialAborted(const base::Feature* iph_feature);

  // Called when the user opts to take a custom action.
  void OnCustomAction(const base::Feature* iph_feature,
                      const UserEducationContextPtr& context,
                      const UserEducationContextPtr& bubble_context,
                      FeaturePromoSpecification::CustomActionCallback callback);

  // Create appropriate buttons for a toast promo that's part of a rotating
  // promo.
  std::vector<HelpBubbleButtonParams> CreateRotatingToastButtons(
      const base::Feature& feature);

  // Create appropriate buttons for a snoozeable promo on the current platform.
  std::vector<HelpBubbleButtonParams> CreateSnoozeButtons(
      const base::Feature& feature,
      bool can_snooze);

  // Create appropriate buttons for a tutorial promo on the current platform.
  std::vector<HelpBubbleButtonParams> CreateTutorialButtons(
      const base::Feature& feature,
      const UserEducationContextPtr& context,
      const UserEducationContextPtr& bubble_context,
      bool can_snooze,
      TutorialIdentifier tutorial_id);

  // Create appropriate buttons for a custom action promo.
  std::vector<HelpBubbleButtonParams> CreateCustomActionButtons(
      const base::Feature& feature,
      const UserEducationContextPtr& context,
      const UserEducationContextPtr& bubble_context,
      const std::u16string& custom_action_caption,
      FeaturePromoSpecification::CustomActionCallback custom_action_callback,
      bool custom_action_is_default,
      int custom_action_dismiss_string_id);

  // The feature promo registry to use.
  const raw_ptr<FeaturePromoRegistry> registry_;

  // Non-null as long as a promo is showing.
  std::unique_ptr<FeaturePromoLifecycle> current_promo_;

  // Policy info about the most recent promo that was shown.
  // Updated when a new promo is shown.
  FeaturePromoPriorityProvider::PromoPriorityInfo last_promo_info_;

  // Promo that is being continued during a tutorial launched from the promo
  // bubble.
  FeaturePromoHandle tutorial_promo_handle_;

  BubbleCloseCallback bubble_closed_callback_;
  base::CallbackListSubscription bubble_closed_subscription_;
  base::CallbackListSubscription custom_ui_result_subscription_;

  const raw_ptr<feature_engagement::Tracker> feature_engagement_tracker_;
  const raw_ptr<HelpBubbleFactoryRegistry> bubble_factory_registry_;
  const raw_ptr<UserEducationStorageService> storage_service_;
  const raw_ptr<FeaturePromoSessionPolicy> session_policy_;
  const raw_ptr<TutorialService> tutorial_service_;
  const raw_ref<ProductMessagingController> product_messaging_controller_;

  // Whether IPH should be allowed to show in an inactive window or app.
  // Should be checked in implementations of CanShowPromo(). Typically only
  // modified in tests.
  static bool active_window_check_blocked_;

  // Private data used for queueing and showing promos.
  std::set<raw_ref<const base::Feature>> attempted_startup_promos_;
  const std::string demo_feature_name_;
  std::unique_ptr<PrivateData> private_;
  bool queue_update_pending_ = false;
  base::RepeatingTimer poller_;

  base::WeakPtrFactory<FeaturePromoControllerImpl> weak_ptr_factory_{this};
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_FEATURE_PROMO_CONTROLLER_IMPL_H_
