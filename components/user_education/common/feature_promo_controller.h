// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_CONTROLLER_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_CONTROLLER_H_

#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <string>

#include "base/auto_reset.h"
#include "base/callback_list.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/user_education/common/feature_promo_data.h"
#include "components/user_education/common/feature_promo_handle.h"
#include "components/user_education/common/feature_promo_lifecycle.h"
#include "components/user_education/common/feature_promo_registry.h"
#include "components/user_education/common/feature_promo_result.h"
#include "components/user_education/common/feature_promo_session_policy.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/common/product_messaging_controller.h"
#include "components/user_education/common/tutorial_identifier.h"

namespace ui {
class AcceleratorProvider;
class TrackedElement;
}  // namespace ui

// Declaring these in the global namespace for testing purposes.
class BrowserFeaturePromoControllerTest;
class FeaturePromoLifecycleUiTest;

namespace user_education {

class HelpBubbleFactoryRegistry;
class FeaturePromoStorageService;
class TutorialService;

// Describes the status of a feature promo.
enum class FeaturePromoStatus {
  kNotRunning,        // The promo is not running or queued.
  kQueuedForStartup,  // The promo is waiting for the FE backend to initialize.
  kBubbleShowing,     // The promo bubble is showing.
  kContinued          // The bubble was closed but the promo is still active.
};

// Enum for client code to specify why a promo should be programmatically ended.
enum class EndFeaturePromoReason {
  // Used to indicate that the user left the flow of the FeaturePromo.
  // For example, this may mean the user ignored a page-specific FeaturePromo
  // by navigating to another page.
  kAbortPromo,

  // Used to indicate that the user interacted with the promoted feature
  // in some meaningful way. For example, if an IPH is anchored to
  // a page action then clicking the page action might indicate that the
  // user engaged with the feature.
  kFeatureEngaged,
};

struct FeaturePromoParams;

// Mostly virtual base class for feature promos; used to mock the interface in
// tests.
class FeaturePromoController {
 public:
  using BubbleCloseCallback = base::OnceClosure;
  using ShowPromoResultCallback =
      base::OnceCallback<void(FeaturePromoResult promo_result)>;

  FeaturePromoController();
  FeaturePromoController(const FeaturePromoController& other) = delete;
  virtual ~FeaturePromoController();
  void operator=(const FeaturePromoController& other) = delete;

  // Queries whether the given promo could be shown at the current moment.
  //
  // In general it is unnecessary to call this method if the intention is to
  // show the promo; just call `MaybeShowPromo()` directly. However, in cases
  // where determining whether to try to show a promo would be prohibitively
  // expensive, this is a slightly less expensive out (but please note that it
  // is not zero cost; a number of prefs and application states do need to be
  // queried).
  //
  // Note that some fields of `params` may be ignored if they are not needed to
  // perform the checks involved.
  virtual FeaturePromoResult CanShowPromo(
      const FeaturePromoParams& params) const = 0;

  // Starts the promo if possible. If a result callback is specified, it will be
  // called with the result of trying to show the promo. In cases where a promo
  // could be queued, the callback may happen significantly later.
  virtual void MaybeShowPromo(FeaturePromoParams params) = 0;

  // Tries to start the promo at a time when the Feature Engagement backend may
  // not yet be initialized. Once it is initialized (which could be
  // immediately), attempts to show the promo and calls
  // `params.startup_callback` with the result. If EndPromo() is called before
  // the promo is shown, the promo is canceled immediately.
  //
  // Returns whether the promo was queued, not whether it was actually shown.
  // A promo may be queued and then not show due to its Feature Engagement
  // conditions not being satisfied. For example, if multiple promos with a
  // session limit of 1 are queued, both may queue successfully, but only one
  // will actually show. If you care about whether the promo is actually shown,
  // set an appropriate `startup_callback`.
  //
  // Note: Since `startup_callback` is asynchronous and can theoretically still
  // be pending after the caller's scope disappears, care must be taken to avoid
  // a UAF on callback; the caller should prefer to either not bind transient
  // objects (e.g. only use the callback for things like UMA logging) or use a
  // weak pointer to avoid this situation.
  //
  // Otherwise, this is identical to MaybeShowPromo().
  virtual bool MaybeShowStartupPromo(FeaturePromoParams params) = 0;

  // Gets the current status of the promo associated with `iph_feature`.
  virtual FeaturePromoStatus GetPromoStatus(
      const base::Feature& iph_feature) const = 0;

  // Gets the feature for the current promo.
  virtual const base::Feature* GetCurrentPromoFeature() const = 0;

  // Gets the specification for a feature promo, if a promo is currently
  // showing anchored to the given element identifier.
  //
  // This is used by menus to continue the promo and highlight menu items
  // when the user opens the menu.
  virtual const FeaturePromoSpecification*
  GetCurrentPromoSpecificationForAnchor(
      ui::ElementIdentifier menu_element_id) const = 0;

  // Returns whether a particular promo has previously been dismissed.
  // Useful in cases where determining if a promo should show could be
  // expensive. If `last_close_reason` is set, and the promo has been
  // dismissed, it wil be populated with the most recent close reason.
  // (The value is undefined if this method returns false.)
  //
  // Note that while `params` is a full parameters block, only `feature` and
  // `key` are actually used.
  virtual bool HasPromoBeenDismissed(
      const FeaturePromoParams& params,
      FeaturePromoClosedReason* last_close_reason = nullptr) const = 0;

  // Returns whether the promo for `iph_feature` matches kBubbleShowing or any
  // of `additional_status`.
  template <typename... Args>
  bool IsPromoActive(const base::Feature& iph_feature,
                     Args... additional_status) const {
    const FeaturePromoStatus actual = GetPromoStatus(iph_feature);
    const std::initializer_list<FeaturePromoStatus> list{additional_status...};
    DCHECK(!base::Contains(list, FeaturePromoStatus::kNotRunning));
    return actual == FeaturePromoStatus::kBubbleShowing ||
           base::Contains(list, actual);
  }

  // Starts a promo with the settings for skipping any logging or filtering
  // provided by the implementation for MaybeShowPromo.
  virtual FeaturePromoResult MaybeShowPromoForDemoPage(
      FeaturePromoParams params) = 0;

  // Ends or cancels the current promo if it is queued. Returns true if a promo
  // was successfully canceled or a bubble closed.
  //
  // Has no effect for promos closed with CloseBubbleAndContinuePromo(); discard
  // or release the FeaturePromoHandle to end those promos.
  virtual bool EndPromo(const base::Feature& iph_feature,
                        EndFeaturePromoReason end_promo_reason) = 0;

  // Closes the promo for `iph_feature` - which must be showing - but continues
  // the promo via the return value. Dispose or release the resulting handle to
  // actually end the promo.
  //
  // Useful when a promo chains into some other user action and you don't want
  // other promos to be able to show until after the operation is finished.
  virtual FeaturePromoHandle CloseBubbleAndContinuePromo(
      const base::Feature& iph_feature) = 0;

  // Returns a weak pointer to this object.
  virtual base::WeakPtr<FeaturePromoController> GetAsWeakPtr() = 0;

  // Posts `result` to `callback` on a fresh call stack. Requires a functioning
  // message pump.
  static void PostShowPromoResult(ShowPromoResultCallback callback,
                                  FeaturePromoResult result);

 protected:
  friend class FeaturePromoHandle;

  // Called when FeaturePromoHandle is destroyed to finish the promo.
  virtual void FinishContinuedPromo(const base::Feature& iph_feature) = 0;

  // Records when and why an IPH was not shown.
  virtual void RecordPromoNotShown(
      const char* feature_name,
      FeaturePromoResult::Failure failure) const = 0;
};

// Manages display of in-product help promos. All IPH displays in Top
// Chrome should go through here.
class FeaturePromoControllerCommon : public FeaturePromoController {
 public:
  using TestLock = std::unique_ptr<base::AutoReset<bool>>;

  FeaturePromoControllerCommon(
      feature_engagement::Tracker* feature_engagement_tracker,
      FeaturePromoRegistry* registry,
      HelpBubbleFactoryRegistry* help_bubble_registry,
      FeaturePromoStorageService* storage_service,
      FeaturePromoSessionPolicy* session_policy,
      TutorialService* tutorial_service,
      ProductMessagingController* messaging_controller);
  ~FeaturePromoControllerCommon() override;

  // For systems where there are rendering issues of e.g. displaying the
  // omnibox and a bubble in the same region on the screen, dismisses a non-
  // critical promo bubble which overlaps a given screen region. Returns true
  // if a bubble is closed as a result.
  bool DismissNonCriticalBubbleInRegion(const gfx::Rect& screen_bounds);

#if !BUILDFLAG(IS_ANDROID)
  // If `feature` has a registered promo, notifies the tracker that the feature
  // has been used.
  void NotifyFeatureUsedIfValid(const base::Feature& feature);
#endif

  // Returns the associated feature engagement tracker.
  feature_engagement::Tracker* feature_engagement_tracker() {
    return feature_engagement_tracker_;
  }

  // FeaturePromoController:
  FeaturePromoResult CanShowPromo(
      const FeaturePromoParams& params) const override;
  void MaybeShowPromo(FeaturePromoParams params) override;
  bool MaybeShowStartupPromo(FeaturePromoParams params) override;
  FeaturePromoStatus GetPromoStatus(
      const base::Feature& iph_feature) const override;
  const FeaturePromoSpecification* GetCurrentPromoSpecificationForAnchor(
      ui::ElementIdentifier menu_element_id) const override;
  bool HasPromoBeenDismissed(
      const FeaturePromoParams& params,
      FeaturePromoClosedReason* close_reason = nullptr) const override;
  FeaturePromoResult MaybeShowPromoForDemoPage(
      FeaturePromoParams params) override;
  bool EndPromo(const base::Feature& iph_feature,
                EndFeaturePromoReason end_promo_reason) override;
  FeaturePromoHandle CloseBubbleAndContinuePromo(
      const base::Feature& iph_feature) final;
  base::WeakPtr<FeaturePromoController> GetAsWeakPtr() override;

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
  friend BrowserFeaturePromoControllerTest;
  friend FeaturePromoLifecycleUiTest;

  enum class ShowSource { kNormal, kQueue, kDemo };

  // Internal entry point for showing a promo.
  FeaturePromoResult MaybeShowPromoImpl(FeaturePromoParams params,
                                        ShowSource source);

  // Common logic for showing feature promos.
  FeaturePromoResult MaybeShowPromoCommon(FeaturePromoParams params,
                                          ShowSource source);

  const FeaturePromoStorageService* storage_service() const {
    return storage_service_;
  }
  FeaturePromoStorageService* storage_service() { return storage_service_; }
  HelpBubble* promo_bubble() {
    return current_promo_ ? current_promo_->help_bubble() : nullptr;
  }
  const HelpBubble* promo_bubble() const {
    return current_promo_ ? current_promo_->help_bubble() : nullptr;
  }

  // Gets the context in which to locate the anchor view.
  virtual ui::ElementContext GetAnchorContext() const = 0;

  // Determine if the current context and anchor element allow showing a promo.
  // This lets us rule out e.g. inactive and incognito windows/apps for
  // non-critical promos.
  //
  // Note: Implementations should make sure to check
  // `active_window_check_blocked()`.
  virtual bool CanShowPromoForElement(
      ui::TrackedElement* anchor_element) const = 0;

  // Get the accelerator provider to use to look up accelerators.
  virtual const ui::AcceleratorProvider* GetAcceleratorProvider() const = 0;

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
  virtual std::u16string GetTutorialScreenReaderHint() const = 0;

  // This method returns an appropriate prompt for promoting using a navigation
  // accelerator to focus the help bubble.
  virtual std::u16string GetFocusHelpBubbleScreenReaderHint(
      FeaturePromoSpecification::PromoType promo_type,
      ui::TrackedElement* anchor_element) const = 0;

  const FeaturePromoRegistry* registry() const { return registry_; }
  FeaturePromoRegistry* registry() { return registry_; }

 private:
  struct ShowPromoBubbleParams;
  struct QueuedPromoData;

  // Note: this data structure is inefficient for lookups, but given that only a
  // small number of promos should be queued at any given point, it's probably
  // still faster than some kind of linked map implementation would be.
  using QueuedPromos = std::list<QueuedPromoData>;

  bool EndPromo(const base::Feature& iph_feature,
                FeaturePromoClosedReason close_reason);
  void RecordPromoEnded(FeaturePromoClosedReason close_reason,
                        bool continue_after_close);

  FeaturePromoHandle CloseBubbleAndContinuePromoWithReason(
      const base::Feature& iph_action,
      FeaturePromoClosedReason close_reason);

  // FeaturePromoController:
  void FinishContinuedPromo(const base::Feature& iph_feature) override;

  // Returns whether we can play a screen reader prompt for the "focus help
  // bubble" promo.
  // TODO(crbug.com/40200981): This must be called *before* we ask if the bubble
  // will show because a limitation in the current FE backend causes
  // ShouldTriggerHelpUI() to always return false if another promo is being
  // displayed. Once we have machinery to allow concurrency in the FE system
  // all of this logic can be rewritten.
  bool CheckScreenReaderPromptAvailable(bool for_demo) const;

  // Handles firing async promos.
  void OnFeatureEngagementTrackerInitialized(
      bool tracker_initialized_successfully);

  // Registers with the ProductMessagingController if not already registered.
  void MaybeRequestMessagePriority();

  // Handles coordination with the product messaging system.
  void OnMessagePriority(RequiredNoticePriorityHandle notice_handle);

  // Returns the next-highest-priority queued promo, or `queued_promos_.end()`
  // if one is not present.
  QueuedPromos::iterator GetNextQueuedPromo();

  // Const version returns a pointer to the queued data, or null if no promos
  // are queued.
  const QueuedPromoData* GetNextQueuedPromo() const;

  // Possibly fires a queued promo based on certain conditions.
  void MaybeShowQueuedPromo();

  // Returns whether `iph_feature` is queued to be shown.
  bool IsPromoQueued(const base::Feature& iph_feature) const;

  // Returns an iterator into the queued promo list matching `iph_feature`, or
  // `queued_promos_.end()` if not found.
  QueuedPromos::iterator FindQueuedPromo(const base::Feature& iph_feature);

  // Fails and clears all queued promos.
  void FailQueuedPromos();

  // Performs common logic for determining if a feature promo for `iph_feature`
  // could be shown right now.
  //
  // The optional parameters `display_spec`, `primary_spec`, `lifecycle`, and
  // `anchor_element` will be populated on success, if specified:
  //  - `primary_spec` - the specification of the promo that has been requested
  //    to be shown; for rotating promos, this is different from the
  //    `display_spec`.
  //  - `display_spec` - the specification of the actual promo to be shown; for
  //    non-rotating promos, this is the same as `primary_spec`.
  //  - `lifecycle` - an object representing the lifecycle of the promo; used to
  //    determine whether the promo can show and record pref and histogram data
  //    when it does.
  //  - `anchor_element` - the UI element the promo should attach to.
  FeaturePromoResult CanShowPromoCommon(
      const FeaturePromoParams& params,
      ShowSource source,
      const FeaturePromoSpecification** primary_spec = nullptr,
      const FeaturePromoSpecification** display_spec = nullptr,
      std::unique_ptr<FeaturePromoLifecycle>* lifecycle = nullptr,
      ui::TrackedElement** anchor_element = nullptr) const;

  // Method that creates the bubble for a feature promo. May return null if the
  // bubble cannot be shown.
  std::unique_ptr<HelpBubble> ShowPromoBubbleImpl(
      ShowPromoBubbleParams show_params);

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
                         TutorialIdentifier tutorial_id);

  // Called when a tutorial launched via StartTutorial() completes.
  void OnTutorialComplete(const base::Feature* iph_feature);

  // Called when a tutorial launched via StartTutorial() aborts.
  void OnTutorialAborted(const base::Feature* iph_feature);

  // Called when the user opts to take a custom action.
  void OnCustomAction(const base::Feature* iph_feature,
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
      bool can_snooze,
      TutorialIdentifier tutorial_id);

  // Create appropriate buttons for a custom action promo.
  std::vector<HelpBubbleButtonParams> CreateCustomActionButtons(
      const base::Feature& feature,
      const std::u16string& custom_action_caption,
      FeaturePromoSpecification::CustomActionCallback custom_action_callback,
      bool custom_action_is_default,
      int custom_action_dismiss_string_id);

  // Records when and why an IPH was not shown.
  void RecordPromoNotShown(const char* feature_name,
                           FeaturePromoResult::Failure failure) const final;

  const base::Feature* GetCurrentPromoFeature() const final;

  // Whether the IPH Demo Mode flag has been set at startup.
  const bool in_iph_demo_mode_;

  // The feature promo registry to use.
  const raw_ptr<FeaturePromoRegistry> registry_;

  // Non-null as long as a promo is showing.
  std::unique_ptr<FeaturePromoLifecycle> current_promo_;

  // Policy info about the most recent promo that was shown.
  // Updated when a new promo is shown.
  FeaturePromoSessionPolicy::PromoInfo last_promo_info_;

  // Promo that is being continued during a tutorial launched from the promo
  // bubble.
  FeaturePromoHandle tutorial_promo_handle_;

  base::OnceClosure bubble_closed_callback_;
  base::CallbackListSubscription bubble_closed_subscription_;

  const raw_ptr<feature_engagement::Tracker> feature_engagement_tracker_;
  const raw_ptr<HelpBubbleFactoryRegistry> bubble_factory_registry_;
  const raw_ptr<FeaturePromoStorageService> storage_service_;
  const raw_ptr<FeaturePromoSessionPolicy> session_policy_;
  const raw_ptr<TutorialService> tutorial_service_;
  const raw_ptr<ProductMessagingController> messaging_controller_;

  // Tracks pending promos that have been queued (e.g. for startup).
  QueuedPromos queued_promos_;

  // Tracks whether this controller has messaging priority.
  RequiredNoticePriorityHandle messaging_priority_handle_;

  base::WeakPtrFactory<FeaturePromoControllerCommon> weak_ptr_factory_{this};

  // Whether IPH should be allowed to show in an inactive window or app.
  // Should be checked in implementations of CanShowPromo(). Typically only
  // modified in tests.
  static bool active_window_check_blocked_;
};

// Params for showing a promo; you can pass a single feature or add additional
// params as necessary. Replaces the old parameter list as it was (a) long and
// unwieldy, and (b) violated the prohibition on optional parameters in virtual
// methods.
struct FeaturePromoParams {
  // NOLINTNEXTLINE(google-explicit-constructor)
  FeaturePromoParams(const base::Feature& iph_feature,
                     const std::string& key = std::string());
  FeaturePromoParams(FeaturePromoParams&& other) noexcept;
  ~FeaturePromoParams();

  // The feature for the IPH to show. Must be an IPH feature defined in
  // components/feature_engagement/public/feature_list.cc and registered with
  // |FeaturePromoRegistry|.
  //
  // Note that this is different than the feature that the IPH is showing for.
  raw_ref<const base::Feature> feature;

  // The key required for keyed promos. Should be left empty for all other
  // (i.e. non-keyed) promos.
  std::string key;

  // Will be called when the promo actually shows or fails to show. For queued
  // promos, will be called when the promo is shown. For non-queued promos, will
  // be posted immediately with the result of the request (arrives on a fresh
  // message loop call stack).
  FeaturePromoController::ShowPromoResultCallback show_promo_result_callback;

  // If a bubble was shown and `close_callback` is provided, it will be called
  // when the bubble closes. The callback must remain valid as long as the
  // bubble shows.
  FeaturePromoController::BubbleCloseCallback close_callback;

  // If the body text is parameterized, pass parameters here.
  FeaturePromoSpecification::FormatParameters body_params =
      FeaturePromoSpecification::NoSubstitution();

  // If the accessible text is parameterized, pass parameters here.
  FeaturePromoSpecification::FormatParameters screen_reader_params =
      FeaturePromoSpecification::NoSubstitution();

  // If the title text is parameterized, pass parameters here.
  FeaturePromoSpecification::FormatParameters title_params =
      FeaturePromoSpecification::NoSubstitution();
};

std::ostream& operator<<(std::ostream& os, FeaturePromoStatus status);

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_CONTROLLER_H_
