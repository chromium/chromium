// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_CONTROLLER_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_CONTROLLER_H_

#include <initializer_list>
#include <map>
#include <memory>
#include <string>

#include "base/auto_reset.h"
#include "base/callback_list.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/user_education/common/feature_promo_handle.h"
#include "components/user_education/common/feature_promo_lifecycle.h"
#include "components/user_education/common/feature_promo_registry.h"
#include "components/user_education/common/feature_promo_result.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/user_education/common/feature_promo_storage_service.h"
#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/help_bubble_params.h"
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
class TutorialService;

// Describes the status of a feature promo.
enum class FeaturePromoStatus {
  kNotRunning,        // The promo is not running or queued.
  kQueuedForStartup,  // The promo is waiting for the FE backend to initialize.
  kBubbleShowing,     // The promo bubble is showing.
  kContinued          // The bubble was closed but the promo is still active.
};

// Public enum to indicate the reason a FeaturePromo was ended. This
// is a subset of the FeaturePromoCloseReasonInternal enum. There are no
// values in this enum because this value only maps to the internal enum
// which is then used to record metrics.
enum class FeaturePromoCloseReason {
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
  using StartupPromoCallback =
      base::OnceCallback<void(const base::Feature& iph_feature,
                              FeaturePromoResult promo_result)>;

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
  virtual FeaturePromoResult CanShowPromo(
      const base::Feature& iph_feature) const = 0;

  // Starts the promo if possible. Returns whether it started.
  // If the Feature Engagement backend is not initialized, returns false.
  virtual FeaturePromoResult MaybeShowPromo(FeaturePromoParams params) = 0;

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

  // Returns whether a particular promo has previously been dismissed.
  // Useful in cases where determining if a promo should show could be
  // expensive. If `last_close_reason` is set, and the promo has been
  // dismissed, it wil be populated with the most recent close reason.
  // (The value is undefined if this method returns false.)
  virtual bool HasPromoBeenDismissed(const base::Feature& iph_feature,
                                     FeaturePromoStorageService::CloseReason*
                                         last_close_reason = nullptr) const = 0;

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
                        FeaturePromoCloseReason close_reason) = 0;

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

 protected:
  friend class FeaturePromoHandle;

  // Called when FeaturePromoHandle is destroyed to finish the promo.
  virtual void FinishContinuedPromo(const base::Feature& iph_feature) = 0;
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
      TutorialService* tutorial_service);
  ~FeaturePromoControllerCommon() override;

  // Only for security or privacy critical promos. Immediately shows a
  // promo with |params|, cancelling any normal promo and blocking any
  // further promos until it's done.
  //
  // Returns an ID that can be passed to CloseBubbleForCriticalPromo()
  // if successful. This can fail if another critical promo is showing.
  std::unique_ptr<HelpBubble> ShowCriticalPromo(
      const FeaturePromoSpecification& spec,
      ui::TrackedElement* anchor_element,
      FeaturePromoSpecification::FormatParameters body_params =
          FeaturePromoSpecification::NoSubstitution(),
      FeaturePromoSpecification::FormatParameters title_params =
          FeaturePromoSpecification::NoSubstitution());

  // For systems where there are rendering issues of e.g. displaying the
  // omnibox and a bubble in the same region on the screen, dismisses a non-
  // critical promo bubble which overlaps a given screen region. Returns true
  // if a bubble is closed as a result.
  bool DismissNonCriticalBubbleInRegion(const gfx::Rect& screen_bounds);

  // Returns the associated feature engagement tracker.
  feature_engagement::Tracker* feature_engagement_tracker() {
    return feature_engagement_tracker_;
  }

  // FeaturePromoController:
  FeaturePromoResult CanShowPromo(
      const base::Feature& iph_feature) const override;
  FeaturePromoResult MaybeShowPromo(FeaturePromoParams params) override;
  bool MaybeShowStartupPromo(FeaturePromoParams params) override;
  FeaturePromoStatus GetPromoStatus(
      const base::Feature& iph_feature) const override;
  bool HasPromoBeenDismissed(const base::Feature& iph_feature,
                             FeaturePromoStorageService::CloseReason*
                                 close_reason = nullptr) const override;
  FeaturePromoResult MaybeShowPromoForDemoPage(
      FeaturePromoParams params) override;
  bool EndPromo(const base::Feature& iph_feature,
                FeaturePromoCloseReason close_reason) override;
  FeaturePromoHandle CloseBubbleAndContinuePromo(
      const base::Feature& iph_feature) final;
  base::WeakPtr<FeaturePromoController> GetAsWeakPtr() override;

  HelpBubbleFactoryRegistry* bubble_factory_registry() {
    return bubble_factory_registry_;
  }

  HelpBubble* promo_bubble_for_testing() { return promo_bubble(); }
  HelpBubble* critical_promo_bubble_for_testing() {
    return critical_promo_bubble();
  }

  TutorialService* tutorial_service_for_testing() { return tutorial_service_; }

  // Blocks a check whether the IPH would be created in an inactive window or
  // app before showing the IPH. Intended for browser and unit tests.
  // The actual implementation of the check is in the platform-specific
  // implementation of CanShowPromo().
  [[nodiscard]] static TestLock BlockActiveWindowCheckForTesting();

 protected:
  friend BrowserFeaturePromoControllerTest;
  friend FeaturePromoLifecycleUiTest;

  // Common logic for showing feature promos.
  FeaturePromoResult MaybeShowPromoCommon(FeaturePromoParams params,
                                          bool for_demo);

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
  HelpBubble* critical_promo_bubble() { return critical_promo_bubble_; }
  const HelpBubble* critical_promo_bubble() const {
    return critical_promo_bubble_;
  }

  // Get the current app ID, if this is an app, empty otherwise.
  virtual std::string GetAppId() const = 0;

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
      ui::TrackedElement* anchor_element,
      bool is_critical_promo) const = 0;

  const FeaturePromoRegistry* registry() const { return registry_; }
  FeaturePromoRegistry* registry() { return registry_; }

  static bool active_window_check_blocked() {
    return active_window_check_blocked_;
  }

 private:
  using CloseReason = FeaturePromoStorageService::CloseReason;

  bool EndPromo(const base::Feature& iph_feature, CloseReason close_reason);

  FeaturePromoHandle CloseBubbleAndContinuePromoWithReason(
      const base::Feature& iph_action,
      CloseReason close_reason);

  // FeaturePromoController:
  void FinishContinuedPromo(const base::Feature& iph_feature) override;

  // Returns whether we can play a screen reader prompt for the "focus help
  // bubble" promo.
  // TODO(crbug.com/1258216): This must be called *before* we ask if the bubble
  // will show because a limitation in the current FE backend causes
  // ShouldTriggerHelpUI() to always return false if another promo is being
  // displayed. Once we have machinery to allow concurrency in the FE system
  // all of this logic can be rewritten.
  bool CheckScreenReaderPromptAvailable(bool for_demo) const;

  // Handles firing async promos.
  void OnFeatureEngagementTrackerInitialized(
      FeaturePromoParams params,
      bool tracker_initialized_successfully);

  // Performs common logic for determining if a feature promo for `iph_feature`
  // could be shown right now.
  //
  // The optional parameters `spec`, `lifecycle`, and `anchor_element` will be
  // populated on success, if specified.
  FeaturePromoResult CanShowPromoCommon(
      const base::Feature& iph_feature,
      bool for_demo,
      const FeaturePromoSpecification** spec = nullptr,
      std::unique_ptr<FeaturePromoLifecycle>* lifecycle = nullptr,
      ui::TrackedElement** anchor_element = nullptr) const;

  // Method that creates the bubble for a feature promo. May return null if the
  // bubble cannot be shown.
  std::unique_ptr<HelpBubble> ShowPromoBubbleImpl(
      const FeaturePromoSpecification& spec,
      ui::TrackedElement* anchor_element,
      FeaturePromoSpecification::FormatParameters body_params,
      FeaturePromoSpecification::FormatParameters title_params,
      bool screen_reader_prompt_available,
      bool is_critical_promo);

  // Callback that cleans up a help bubble when it is closed.
  void OnHelpBubbleClosed(HelpBubble* bubble);

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

  // Create appropriate buttons for a snoozeable promo on the current platform.
  std::vector<HelpBubbleButtonParams> CreateSnoozeButtons(
      const base::Feature& feature);

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

  const base::Feature* GetCurrentPromoFeature() const;

  // Whether the IPH Demo Mode flag has been set at startup.
  const bool in_iph_demo_mode_;

  // The feature promo registry to use.
  const raw_ptr<FeaturePromoRegistry> registry_;

  // Non-null as long as a promo is showing.
  std::unique_ptr<FeaturePromoLifecycle> current_promo_;

  // Has a value if a critical promo is showing. If this has a value,
  // |current_iph_feature_| will usually be null. There is one edge case
  // where this may not be true: when a critical promo is requested
  // between a normal promo's CloseBubbleAndContinuePromo() call and its
  // end.
  raw_ptr<HelpBubble> critical_promo_bubble_ = nullptr;

  // Promo that is being continued during a tutorial launched from the promo
  // bubble.
  FeaturePromoHandle tutorial_promo_handle_;

  base::OnceClosure bubble_closed_callback_;
  base::CallbackListSubscription bubble_closed_subscription_;

  const raw_ptr<feature_engagement::Tracker> feature_engagement_tracker_;
  const raw_ptr<HelpBubbleFactoryRegistry> bubble_factory_registry_;
  const raw_ptr<FeaturePromoStorageService> storage_service_;
  const raw_ptr<TutorialService> tutorial_service_;

  // Tracks pending startup promos that have not been canceled.
  std::map<const base::Feature*, StartupPromoCallback> startup_promos_;

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
  FeaturePromoParams(const base::Feature& iph_feature);
  FeaturePromoParams(FeaturePromoParams&& other);
  ~FeaturePromoParams();

  // The feature for the IPH to show. Must be an IPH feature defined in
  // components/feature_engagement/public/feature_list.cc and registered with
  // |FeaturePromoRegistry|.
  //
  // Note that this is different than the feature that the IPH is showing for.
  raw_ref<const base::Feature> feature;

  // Used for startup promos; will be called when the promo actually shows.
  FeaturePromoController::StartupPromoCallback startup_callback;

  // If a bubble was shown and `close_callback` is provided, it will be called
  // when the bubble closes. The callback must remain valid as long as the
  // bubble shows.
  FeaturePromoController::BubbleCloseCallback close_callback;

  // If the body text is parameterized, pass parameters here.
  FeaturePromoSpecification::FormatParameters body_params =
      FeaturePromoSpecification::NoSubstitution();

  // If the title text is parameterized, pass parameters here.
  FeaturePromoSpecification::FormatParameters title_params =
      FeaturePromoSpecification::NoSubstitution();
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_CONTROLLER_H_
