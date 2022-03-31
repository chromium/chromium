// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_FEATURE_PROMO_CONTROLLER_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_FEATURE_PROMO_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/callback.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/user_education/feature_promo_registry.h"
#include "chrome/browser/ui/user_education/feature_promo_specification.h"
#include "chrome/browser/ui/user_education/help_bubble.h"
#include "chrome/browser/ui/user_education/help_bubble_params.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_identifier.h"
#include "components/feature_engagement/public/tracker.h"

namespace base {
struct Feature;
}

namespace ui {
class AcceleratorProvider;
class TrackedElement;
}  // namespace ui

class FeaturePromoSnoozeService;
class HelpBubbleFactoryRegistry;
class TutorialService;

// Mostly virtual base class for feature promos; used to mock the interface in
// tests.
class FeaturePromoController {
 public:
  using BubbleCloseCallback = base::OnceClosure;

  // Represents a promo that has been continued after its bubble has been
  // hidden, as a result of calling CloseBubbleAndContinuePromo().
  //
  // The promo is considered still active until the handle is released or
  // destroyed and no other promos will be allowed to show.
  //
  // PromoHandle is a value-typed, movable smart reference; default constructed
  // instances are falsy (i.e. operator bool and is_valid() return false), as
  // are any instances that have been moved or released.
  class PromoHandle {
   public:
    PromoHandle();
    PromoHandle(base::WeakPtr<FeaturePromoController> controller,
                const base::Feature* feature);
    PromoHandle(PromoHandle&&);
    ~PromoHandle();

    PromoHandle& operator=(PromoHandle&&);

    explicit operator bool() const { return is_valid(); }
    bool operator!() const { return !is_valid(); }

    // Returns whether the handle refers to a valid promo. Returns null for
    // default-constructed objects and after being moved or released.
    bool is_valid() const { return feature_; }

    // Releases the promo and resets the handle. After release, operator bool
    // will return false regardless of the previous state.
    void Release();

   private:
    base::WeakPtr<FeaturePromoController> controller_;
    const base::Feature* feature_ = nullptr;
  };

  FeaturePromoController();
  FeaturePromoController(const FeaturePromoController& other) = delete;
  virtual ~FeaturePromoController();
  void operator=(const FeaturePromoController& other) = delete;

  // Starts the promo if possible. Returns whether it started.
  // |iph_feature| must be an IPH feature defined in
  // components/feature_engagement/public/feature_list.cc and registered
  // with |FeaturePromoRegistry|. Note that this is different than the
  // feature that the IPH is showing for.
  //
  // If the body text is parameterized, pass text replacements in
  // |body_text_replacements|.
  //
  // If a bubble was shown and |close_callback| was provided, it will be
  // called when the bubble closes. |close_callback| must be valid as
  // long as the bubble shows.
  //
  // For users that can't register their parameters with
  // FeaturePromoRegistry, see
  // |FeaturePromoControllerViews::MaybeShowPromoWithParams()|. Prefer
  // statically registering params with FeaturePromoRegistry and using
  // this method when possible.
  virtual bool MaybeShowPromo(
      const base::Feature& iph_feature,
      FeaturePromoSpecification::StringReplacements body_text_replacements = {},
      BubbleCloseCallback close_callback = BubbleCloseCallback()) = 0;

  // Returns whether a bubble is showing for the given promo. If
  // `include_continued_promos` is set, also returns true if a promo bubble has
  // been hidden with CloseBubbleAndContinuePromo() but the promo is still
  // active in the background.
  virtual bool IsPromoActive(const base::Feature& iph_feature,
                             bool include_continued_promos) const = 0;

  // Starts a promo with the settings for skipping any logging or filtering
  // provided by the implementation for MaybeShowPromo.
  virtual bool MaybeShowPromoForDemoPage(
      const base::Feature* iph_feature,
      FeaturePromoSpecification::StringReplacements body_text_replacements = {},
      BubbleCloseCallback close_callback = BubbleCloseCallback()) = 0;

  // If a bubble is showing for |iph_feature| close it and end the
  // promo. Does nothing otherwise. Returns true if a bubble was closed
  // and false otherwise.
  //
  // Calling this has no effect if |CloseBubbleAndContinuePromo()| was
  // called for |iph_feature|.
  virtual bool CloseBubble(const base::Feature& iph_feature) = 0;

  // Like CloseBubble() but does not end the promo yet. The caller takes
  // ownership of the promo (e.g. to show a highlight in a menu or on a
  // button). The returned PromoHandle represents this ownership.
  virtual PromoHandle CloseBubbleAndContinuePromo(
      const base::Feature& iph_feature) = 0;

  // Returns a weak pointer to this object.
  virtual base::WeakPtr<FeaturePromoController> GetAsWeakPtr() = 0;

 protected:
  // Called when PromoHandle is destroyed to finish the promo.
  virtual void FinishContinuedPromo(const base::Feature* iph_feature) = 0;
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
      FeaturePromoSnoozeService* snooze_service,
      TutorialService* tutorial_service);
  ~FeaturePromoControllerCommon() override;

  // Only for security or privacy critical promos. Immedialy shows a
  // promo with |params|, cancelling any normal promo and blocking any
  // further promos until it's done.
  //
  // Returns an ID that can be passed to CloseBubbleForCriticalPromo()
  // if successful. This can fail if another critical promo is showing.
  std::unique_ptr<HelpBubble> ShowCriticalPromo(
      const FeaturePromoSpecification& spec,
      ui::TrackedElement* anchor_element,
      FeaturePromoSpecification::StringReplacements body_text_replacements =
          {});

  // For systems where there are rendering issues of e.g. displaying the
  // omnibox and a bubble in the same region on the screen, dismisses a non-
  // critical promo bubble which overlaps a given screen region. Returns true
  // if a bubble is closed as a result.
  bool DismissNonCriticalBubbleInRegion(const gfx::Rect& screen_bounds);

  // Blocks further promos and closes any existing non-critical ones.
  [[nodiscard]] TestLock BlockPromosForTesting();

  // Returns the associated feature engagement tracker.
  feature_engagement::Tracker* feature_engagement_tracker() {
    return feature_engagement_tracker_;
  }

  // FeaturePromoController:
  bool MaybeShowPromo(
      const base::Feature& iph_feature,
      FeaturePromoSpecification::StringReplacements body_text_replacements = {},
      BubbleCloseCallback close_callback = BubbleCloseCallback()) override;
  bool IsPromoActive(const base::Feature& iph_feature,
                     bool include_continued_promos = false) const override;
  bool MaybeShowPromoForDemoPage(
      const base::Feature* iph_feature,
      FeaturePromoSpecification::StringReplacements body_text_replacements = {},
      BubbleCloseCallback close_callback = BubbleCloseCallback()) override;
  bool CloseBubble(const base::Feature& iph_feature) override;
  PromoHandle CloseBubbleAndContinuePromo(
      const base::Feature& iph_feature) override;
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
  friend class BrowserFeaturePromoControllerTest;
  friend class FeaturePromoSnoozeInteractiveTest;

  // For IPH not registered with |FeaturePromoRegistry|. Only use this
  // if it is infeasible to pre-register your IPH.
  bool MaybeShowPromoFromSpecification(
      const FeaturePromoSpecification& spec,
      ui::TrackedElement* anchor_element,
      FeaturePromoSpecification::StringReplacements body_text_replacements,
      BubbleCloseCallback close_callback);

  FeaturePromoSnoozeService* snooze_service() { return snooze_service_; }
  HelpBubble* promo_bubble() { return promo_bubble_.get(); }
  const HelpBubble* promo_bubble() const { return promo_bubble_.get(); }
  HelpBubble* critical_promo_bubble() { return critical_promo_bubble_; }
  const HelpBubble* critical_promo_bubble() const {
    return critical_promo_bubble_;
  }

  // Gets the context in which to locate the anchor view.
  virtual ui::ElementContext GetAnchorContext() const = 0;

  // Determine if the current context and anchor element allow showing a promo.
  // This lets us rule out e.g. inactive and incognito windows/apps for
  // non-critical promos.
  //
  // Note: Implementations should make sure to check
  // active_window_check_blocked().
  virtual bool CanShowPromo(ui::TrackedElement* anchor_element) const = 0;

  // Get the accelerator provider to use to look up accelerators.
  virtual const ui::AcceleratorProvider* GetAcceleratorProvider() const = 0;

  // These methods control how snooze buttons appear and function.
  virtual std::u16string GetSnoozeButtonText() const = 0;
  virtual std::u16string GetDismissButtonText() const = 0;

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

  FeaturePromoRegistry* registry() { return registry_; }

  static bool active_window_check_blocked() {
    return active_window_check_blocked_;
  }

 private:
  // FeaturePromoController:
  void FinishContinuedPromo(const base::Feature* iph_feature) override;

  // Returns whether we can play a screen reader prompt for the "focus help
  // bubble" promo.
  // TODO(crbug.com/1258216): This must be called *before* we ask if the bubble
  // will show because a limitation in the current FE backend causes
  // ShouldTriggerHelpUI() to always return false if another promo is being
  // displayed. Once we have machinery to allow concurrency in the FE system
  // all of this logic can be rewritten.
  bool CheckScreenReaderPromptAvailable() const;

  // Method that creates the bubble for a feature promo. May return null if the
  // bubble cannot be shown.
  std::unique_ptr<HelpBubble> ShowPromoBubbleImpl(
      const FeaturePromoSpecification& spec,
      ui::TrackedElement* anchor_element,
      FeaturePromoSpecification::StringReplacements body_text_replacements,
      bool screen_reader_prompt_available,
      bool is_critical_promo);

  // Callback that cleans up a help bubble when it is closed.
  void OnHelpBubbleClosed(HelpBubble* bubble);

  // Callback for snoozed features.
  void OnHelpBubbleSnoozed(const base::Feature* feature);

  // Callback when a feature's help bubble is dismissed by any means other than
  // snoozing (including "OK" or "Got it!" buttons).
  void OnHelpBubbleDismissed(const base::Feature* feature);

  // Callback when a tutorial triggered from a promo is actually started.
  void OnTutorialStarted(const base::Feature* iph_feature,
                         TutorialIdentifier tutorial_id);

  // Called when a tutorial launched via StartTutorial() completes.
  void OnTutorialComplete(const base::Feature* iph_feature);

  // Called when a tutorial launched via StartTutorial() aborts.
  void OnTutorialAborted(const base::Feature* iph_feature);

  // Create appropriate buttons for a snoozable promo on the current platform.
  std::vector<HelpBubbleButtonParams> CreateSnoozeButtons(
      const base::Feature& feature);

  // Create appropriate buttons for a tutorial promo on the current platform.
  std::vector<HelpBubbleButtonParams> CreateTutorialButtons(
      const base::Feature& feature,
      TutorialIdentifier tutorial_id);

  // The feature promo registry to use.
  FeaturePromoRegistry* const registry_;

  // Non-null as long as a promo is showing. Corresponds to an IPH
  // feature registered with |feature_engagement_tracker_|.
  raw_ptr<const base::Feature> current_iph_feature_ = nullptr;
  bool continuing_after_bubble_closed_ = false;

  // The help bubble, if a feature promo bubble is showing.
  std::unique_ptr<HelpBubble> promo_bubble_;

  // Has a value if a critical promo is showing. If this has a value,
  // |current_iph_feature_| will usually be null. There is one edge case
  // where this may not be true: when a critical promo is requested
  // between a normal promo's CloseBubbleAndContinuePromo() call and its
  // end.
  raw_ptr<HelpBubble> critical_promo_bubble_ = nullptr;

  // Promo that is being continued during a tutorial launched from the promo
  // bubble.
  PromoHandle tutorial_promo_handle_;

  base::OnceClosure bubble_closed_callback_;
  base::CallbackListSubscription bubble_closed_subscription_;

  const raw_ptr<feature_engagement::Tracker> feature_engagement_tracker_;
  const raw_ptr<HelpBubbleFactoryRegistry> bubble_factory_registry_;
  const raw_ptr<FeaturePromoSnoozeService> snooze_service_;
  const raw_ptr<TutorialService> tutorial_service_;

  // When set to true, promos will never be shown.
  bool promos_blocked_for_testing_ = false;

  // In the case where the user education demo page wants to bypass the feature
  // engagement tracker, the current iph feature will be set and then checked
  // against to verify the right feature is bypassing. this page is located at
  // internals/user-education.
  const base::Feature* iph_feature_bypassing_tracker_ = nullptr;

  base::WeakPtrFactory<FeaturePromoControllerCommon> weak_ptr_factory_{this};

  // Whether IPH should be allowed to show in an inactive window or app.
  // Should be checked in implementations of CanShowPromo(). Typically only
  // modified in tests.
  static bool active_window_check_blocked_;
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_FEATURE_PROMO_CONTROLLER_H_
