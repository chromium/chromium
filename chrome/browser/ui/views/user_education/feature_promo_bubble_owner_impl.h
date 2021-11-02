// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_BUBBLE_OWNER_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_BUBBLE_OWNER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/token.h"
#include "chrome/browser/ui/views/user_education/feature_promo_bubble_owner.h"
#include "chrome/browser/ui/views/user_education/feature_promo_bubble_view.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget_observer.h"

// FeaturePromoBubbleOwner that creates a production FeaturePromoBubbleView.
class FeaturePromoBubbleOwnerImpl : public FeaturePromoBubbleOwner,
                                    views::WidgetObserver {
 public:
  FeaturePromoBubbleOwnerImpl();
  ~FeaturePromoBubbleOwnerImpl() override;

  static FeaturePromoBubbleOwnerImpl* GetInstance();

  // Attempts to activate the bubble, if it's showing, or if it's already
  // focused, attempts to focus the anchor view. Returns true if it was
  // successful. Returns false if no bubble is showing or if focus cannot be
  // toggled.
  bool ToggleFocusForAccessibility();

  // Checks if the bubble is a promo bubble.
  bool IsPromoBubble(const views::DialogDelegate* bubble) const;

  FeaturePromoBubbleView* bubble_for_testing() { return bubble_; }

  // FeaturePromoBubbleOwner:
  absl::optional<base::Token> ShowBubble(
      FeaturePromoBubbleView::CreateParams params,
      base::OnceClosure close_callback) override;
  bool BubbleIsShowing(base::Token bubble_id) const override;
  bool AnyBubbleIsShowing() const override;
  void CloseBubble(base::Token bubble_id) override;
  void NotifyAnchorBoundsChanged() override;
  gfx::Rect GetBubbleBoundsInScreen(base::Token bubble_id) const override;

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  void HandleBubbleClosed();

  // The currently showing bubble, or `nullptr`.
  raw_ptr<FeaturePromoBubbleView> bubble_ = nullptr;

  // ID of the currently showing bubble. Must be nullopt if `bubble_` is null.
  absl::optional<base::Token> bubble_id_;

  // Called when `bubble_` closes.
  base::OnceClosure close_callback_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_BUBBLE_OWNER_IMPL_H_
