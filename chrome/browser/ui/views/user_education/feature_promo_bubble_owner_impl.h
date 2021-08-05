// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_BUBBLE_OWNER_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_BUBBLE_OWNER_IMPL_H_

#include "base/scoped_observation.h"
#include "base/token.h"
#include "chrome/browser/ui/views/user_education/feature_promo_bubble_owner.h"
#include "chrome/browser/ui/views/user_education/feature_promo_bubble_view.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/widget/widget_observer.h"

// FeaturePromoBubbleOwner that creates a production FeaturePromoBubbleView.
class FeaturePromoBubbleOwnerImpl : public FeaturePromoBubbleOwner,
                                    views::WidgetObserver {
 public:
  FeaturePromoBubbleOwnerImpl();
  ~FeaturePromoBubbleOwnerImpl() override;

  static FeaturePromoBubbleOwnerImpl* GetInstance();

  // Attempts to activate the bubble, if it's showing. Returns true if it was
  // activated. Returns false if no bubble is showing or it's not activatable.
  bool ActivateBubbleForAccessibility();

  FeaturePromoBubbleView* bubble_for_testing() { return bubble_; }

  // FeaturePromoBubbleOwner:
  absl::optional<base::Token> ShowBubble(
      FeaturePromoBubbleView::CreateParams params,
      base::OnceClosure close_callback) override;
  bool BubbleIsShowing(base::Token bubble_id) override;
  bool AnyBubbleIsShowing() override;
  void CloseBubble(base::Token bubble_id) override;
  void NotifyAnchorBoundsChanged() override;

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  void HandleBubbleClosed();

  // The currently showing bubble, or `nullptr`.
  FeaturePromoBubbleView* bubble_ = nullptr;

  // ID of the currently showing bubble. Must be nullopt if `bubble_` is null.
  absl::optional<base::Token> bubble_id_;

  // Called when `bubble_` closes.
  base::OnceClosure close_callback_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_BUBBLE_OWNER_IMPL_H_
