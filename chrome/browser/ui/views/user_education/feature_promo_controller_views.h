// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_CONTROLLER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_CONTROLLER_VIEWS_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/token.h"
#include "chrome/browser/ui/user_education/feature_promo_controller.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class BrowserView;
class FeaturePromoBubbleView;
struct FeaturePromoBubbleParams;
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
class FeaturePromoControllerViews : public FeaturePromoController,
                                    public views::WidgetObserver {
 public:
  // Create the instance for the given |browser_view|.
  explicit FeaturePromoControllerViews(BrowserView* browser_view);
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
  bool MaybeShowPromoWithParams(
      const base::Feature& iph_feature,
      const FeaturePromoBubbleParams& params,
      BubbleCloseCallback close_callback = BubbleCloseCallback());

  // Only for security or privacy critical promos. Immedialy shows a
  // promo with |params|, cancelling any normal promo and blocking any
  // further promos until it's done.
  //
  // Returns an ID that can be passed to CloseBubbleForCriticalPromo()
  // if successful. This can fail if another critical promo is showing.
  base::Optional<base::Token> ShowCriticalPromo(
      const FeaturePromoBubbleParams& params);

  // Ends a promo started by ShowCriticalPromo() if it's still showing.
  void CloseBubbleForCriticalPromo(const base::Token& critical_promo_id);

  // FeaturePromoController:
  bool MaybeShowPromo(
      const base::Feature& iph_feature,
      BubbleCloseCallback close_callback = BubbleCloseCallback()) override;
  bool MaybeShowPromoWithTextReplacements(
      const base::Feature& iph_feature,
      FeaturePromoTextReplacements text_replacements,
      BubbleCloseCallback close_callback = BubbleCloseCallback()) override;
  bool BubbleIsShowing(const base::Feature& iph_feature) const override;
  bool CloseBubble(const base::Feature& iph_feature) override;
  PromoHandle CloseBubbleAndContinuePromo(
      const base::Feature& iph_feature) override;

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  // Gets the IPH backend. Provided for convenience.
  feature_engagement::Tracker* feature_engagement_tracker() { return tracker_; }

  // Blocks any further promos from showing. Additionally cancels the
  // current promo unless an outstanding PromoHandle from
  // CloseBubbleAndContinuePromo exists. Intended for browser tests.
  void BlockPromosForTesting();

  FeaturePromoBubbleView* promo_bubble_for_testing() { return promo_bubble_; }
  const FeaturePromoBubbleView* promo_bubble_for_testing() const {
    return promo_bubble_;
  }

  FeaturePromoSnoozeService* snooze_service_for_testing() {
    return snooze_service_.get();
  }

 private:
  bool MaybeShowPromoImpl(const base::Feature& iph_feature,
                          const FeaturePromoBubbleParams& params,
                          BubbleCloseCallback close_callback);

  // Called when PromoHandle is destroyed to finish the promo.
  void FinishContinuedPromo() override;

  void ShowPromoBubbleImpl(const FeaturePromoBubbleParams& params);

  void HandleBubbleClosed();

  // Call these methods when the user actively snooze or dismiss the IPH.
  void OnUserSnooze(const base::Feature& iph_feature);
  void OnUserDismiss(const base::Feature& iph_feature);

  // The browser window this instance is responsible for.
  BrowserView* const browser_view_;

  // Snooze service that is notified when a user snoozes or dismisses the promo.
  // Ask this service for display permission before |tracker_|.
  std::unique_ptr<FeaturePromoSnoozeService> snooze_service_;

  // IPH backend that is notified of user events and decides whether to
  // trigger IPH.
  feature_engagement::Tracker* const tracker_;

  // Non-null as long as a promo is showing. Corresponds to an IPH
  // feature registered with |tracker_|.
  const base::Feature* current_iph_feature_ = nullptr;

  // Has a value if a critical promo is showing. If this has a value,
  // |current_iph_feature_| will usually be null. There is one edge case
  // where this may not be true: when a critical promo is requested
  // between a normal promo's CloseBubbleAndContinuePromo() call and its
  // end.
  base::Optional<base::Token> current_critical_promo_;

  // The bubble currently showing, if any.
  FeaturePromoBubbleView* promo_bubble_ = nullptr;

  // If present, called when |current_iph_feature_|'s bubble stops
  // showing. Only valid if |current_iph_feature_| and |promo_bubble_|
  // are both non-null.
  BubbleCloseCallback close_callback_;

  // Stores the bubble anchor view so we can set/unset a highlight on
  // it.
  views::ViewTracker anchor_view_tracker_;

  bool promos_blocked_for_testing_ = false;

  ScopedObserver<views::Widget, views::WidgetObserver> widget_observer_{this};

  base::WeakPtrFactory<FeaturePromoControllerViews> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_CONTROLLER_VIEWS_H_
