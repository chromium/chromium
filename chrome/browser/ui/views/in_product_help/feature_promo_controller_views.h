// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_IN_PRODUCT_HELP_FEATURE_PROMO_CONTROLLER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_IN_PRODUCT_HELP_FEATURE_PROMO_CONTROLLER_VIEWS_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/in_product_help/feature_promo_controller.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class BrowserView;
class FeaturePromoBubbleView;
struct FeaturePromoBubbleParams;
class FeaturePromoSnoozeService;

namespace base {
struct Feature;
}

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

  // Repositions the bubble (if showing) relative to the anchor view.
  // This should be called whenever the anchor view is potentially
  // moved. It is safe to call this if a bubble is not showing.
  void UpdateBubbleForAnchorBoundsChange();

  // For IPH not registered with |FeaturePromoRegistry|. Only use this
  // if it is infeasible to pre-register your IPH.
  bool MaybeShowPromoWithParams(const base::Feature& iph_feature,
                                const FeaturePromoBubbleParams& params);

  // FeaturePromoController:
  bool MaybeShowPromo(const base::Feature& iph_feature) override;
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
  // Called when PromoHandle is destroyed to finish the promo.
  void FinishContinuedPromo() override;

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

  // The bubble currently showing, if any.
  FeaturePromoBubbleView* promo_bubble_ = nullptr;

  // Stores the bubble anchor view so we can set/unset a highlight on
  // it.
  views::ViewTracker anchor_view_tracker_;

  bool promos_blocked_for_testing_ = false;

  ScopedObserver<views::Widget, views::WidgetObserver> widget_observer_{this};

  base::WeakPtrFactory<FeaturePromoController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_IN_PRODUCT_HELP_FEATURE_PROMO_CONTROLLER_VIEWS_H_
