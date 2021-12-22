// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_CONTROLLER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_CONTROLLER_VIEWS_H_

#include <memory>

#include "base/cancelable_callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/token.h"
#include "chrome/browser/ui/user_education/feature_promo_controller.h"
#include "chrome/browser/ui/user_education/feature_promo_specification.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_identifier.h"
#include "chrome/browser/ui/views/user_education/feature_promo_bubble_owner.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class BrowserView;
class FeaturePromoSnoozeService;

namespace base {
struct Feature;
class Token;
}  // namespace base

namespace feature_engagement {
class Tracker;
}

// Views implementation of FeaturePromoController. There is one instance
// per window.
class FeaturePromoControllerViews : public FeaturePromoController {
 public:
  // Create the instance for the given |browser_view|.
  explicit FeaturePromoControllerViews(BrowserView* browser_view,
                                       FeaturePromoBubbleOwner* bubble_owner,
                                       TutorialService* tutorial_service);
  ~FeaturePromoControllerViews() override;

  // Get the appropriate instance for |view|. This finds the BrowserView
  // that contains |view| and returns its instance. May return nullptr,
  // but if |view| is in a BrowserView's hierarchy it shouldn't.
  static FeaturePromoControllerViews* GetForView(views::View* view);

  // Repositions the bubble (if showing) relative to the anchor view.
  // This should be called whenever the anchor view is potentially
  // moved. It is safe to call this if a bubble is not showing.
  void UpdateBubbleForAnchorBoundsChange();

  // For IPH not registered with |FeaturePromoRegistry|. Only use this
  // if it is infeasible to pre-register your IPH.
  bool MaybeShowPromoFromSpecification(
      const FeaturePromoSpecification& spec,
      views::View* anchor_view,
      FeaturePromoSpecification::StringReplacements body_text_replacements = {},
      BubbleCloseCallback close_callback = BubbleCloseCallback());

  // Builds the CreateParams from the BubbleParams.
  FeaturePromoBubbleView::CreateParams GetBaseCreateParams(
      const FeaturePromoSpecification& spec,
      views::View* anchor_view,
      FeaturePromoSpecification::StringReplacements body_text_replacements,
      bool is_critical_promo);

  // Only for security or privacy critical promos. Immedialy shows a
  // promo with |params|, cancelling any normal promo and blocking any
  // further promos until it's done.
  //
  // Returns an ID that can be passed to CloseBubbleForCriticalPromo()
  // if successful. This can fail if another critical promo is showing.
  absl::optional<base::Token> ShowCriticalPromo(
      const FeaturePromoSpecification& spec,
      views::View* anchor_view,
      FeaturePromoSpecification::StringReplacements body_text_replacements =
          {});

  // Ends a promo started by ShowCriticalPromo() if it's still showing.
  void CloseBubbleForCriticalPromo(const base::Token& critical_promo_id);

  // Returns whether a critical promo is showing for the given `Token`.
  bool CriticalPromoIsShowing(const base::Token& critical_promo_id) const;

  // For systems where there are rendering issues of e.g. displaying the
  // omnibox and a bubble in the same region on the screen, dismisses a non-
  // critical promo bubble which overlaps a given screen region. Returns true
  // if a bubble is closed as a result.
  bool DismissNonCriticalBubbleInRegion(const gfx::Rect& screen_bounds);

  // FeaturePromoController:
  bool MaybeShowPromo(
      const base::Feature& iph_feature,
      FeaturePromoSpecification::StringReplacements body_text_replacements = {},
      BubbleCloseCallback close_callback = BubbleCloseCallback()) override;
  bool BubbleIsShowing(const base::Feature& iph_feature) const override;
  bool CloseBubble(const base::Feature& iph_feature) override;
  PromoHandle CloseBubbleAndContinuePromo(
      const base::Feature& iph_feature) override;
  base::WeakPtr<FeaturePromoController> GetAsWeakPtr() override;

  // Gets the IPH backend. Provided for convenience.
  feature_engagement::Tracker* feature_engagement_tracker() { return tracker_; }

  // Blocks a check that the anchor view for the IPH is in an active window
  // before showing the IPH. Intended for browser and unit tests.
  static void BlockActiveWindowCheckForTesting();

  // Returns true if the IPH should be allowed to show in an inactive window.
  // False by default, but browser and unit tests may modify this behavior.
  static bool IsActiveWindowCheckBlockedForTesting();

  // Blocks any further promos from showing. Additionally cancels the
  // current promo unless an outstanding PromoHandle from
  // CloseBubbleAndContinuePromo exists. Intended for browser tests.
  void BlockPromosForTesting();

  FeaturePromoSnoozeService* snooze_service_for_testing() {
    return snooze_service_.get();
  }

  FeaturePromoBubbleOwner* bubble_owner_for_testing() {
    return bubble_owner_.get();
  }

 private:
  bool MaybeShowPromoImpl(
      const FeaturePromoSpecification& spec,
      views::View* anchor_view,
      FeaturePromoSpecification::StringReplacements text_replacements,
      BubbleCloseCallback close_callback);

  // Called when PromoHandle is destroyed to finish the promo.
  void FinishContinuedPromo() override;

  bool ShowPromoBubbleImpl(
      const FeaturePromoSpecification& spec,
      views::View* anchor_view,
      FeaturePromoSpecification::StringReplacements text_replacements,
      bool screen_reader_promo,
      bool is_critical_promo);

  void HandleBubbleClosed();

  // Call these methods when the user actively snooze or dismiss the IPH.
  void OnUserSnooze(const base::Feature& iph_feature);
  void OnUserDismiss(const base::Feature& iph_feature);
  void OnTutorialStart(const base::Feature& iph_feature,
                       TutorialIdentifier tutorial_id);

  // Launch a tutorial.
  void StartTutorial(TutorialIdentifier tutorial_id);

  // Returns whether we can play a screen reader prompt for the "focus help
  // bubble" promo.
  // TODO(crbug.com/1258216): This must be called *before* we ask if the bubble
  // will show because a limitation in the current FE backend causes
  // ShouldTriggerHelpUI() to always return false if another promo is being
  // displayed. Once we have machinery to allow concurrency in the FE system
  // all of this logic can be rewritten.
  bool CheckScreenReaderPromptAvailable() const;

  // Create appropriate buttons for a snooze promo for the current platform.
  std::vector<FeaturePromoBubbleView::ButtonParams> CreateSnoozeButtons(
      const base::Feature& feature);

  // Create appropriate buttons for a tutorial promo for the current platform.
  std::vector<FeaturePromoBubbleView::ButtonParams> CreateTutorialButtons(
      const base::Feature& feature,
      TutorialIdentifier tutorial_id);

  // The browser window this instance is responsible for.
  const raw_ptr<BrowserView> browser_view_;

  // The delegate responsible for creating and owning a bubble.
  const raw_ptr<FeaturePromoBubbleOwner> bubble_owner_;

  // Snooze service that is notified when a user snoozes or dismisses the promo.
  // Ask this service for display permission before |tracker_|.
  std::unique_ptr<FeaturePromoSnoozeService> snooze_service_;

  // The tutorial service to use to launch tutorials.
  TutorialService* const tutorial_service_;

  // IPH backend that is notified of user events and decides whether to
  // trigger IPH.
  const raw_ptr<feature_engagement::Tracker> tracker_;

  // Non-null as long as a promo is showing. Corresponds to an IPH
  // feature registered with |tracker_|.
  raw_ptr<const base::Feature> current_iph_feature_ = nullptr;

  // Bubble ID from `bubble_owner_`, if a bubble is showing.
  absl::optional<base::Token> bubble_id_;

  // Has a value if a critical promo is showing. If this has a value,
  // |current_iph_feature_| will usually be null. There is one edge case
  // where this may not be true: when a critical promo is requested
  // between a normal promo's CloseBubbleAndContinuePromo() call and its
  // end.
  absl::optional<base::Token> current_critical_promo_;

  // If present, called when |current_iph_feature_|'s bubble stops
  // showing. Only valid if |current_iph_feature_| and |promo_bubble_|
  // are both non-null.
  BubbleCloseCallback close_callback_;

  // Stores the bubble anchor view so we can set/unset a highlight on
  // it.
  views::ViewTracker anchor_view_tracker_;

  // Pending tutorial to run, if any.
  TutorialIdentifier pending_tutorial_;

  static bool active_window_check_blocked_for_testing;
  bool promos_blocked_for_testing_ = false;

  base::WeakPtrFactory<FeaturePromoControllerViews> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_CONTROLLER_VIEWS_H_
