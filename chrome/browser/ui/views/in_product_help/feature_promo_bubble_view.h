// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_IN_PRODUCT_HELP_FEATURE_PROMO_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_IN_PRODUCT_HELP_FEATURE_PROMO_BUBBLE_VIEW_H_

#include <cstddef>
#include <memory>

#include "base/optional.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_bubble_params.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_bubble_timeout.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace gfx {
class Rect;
}

namespace ui {
class MouseEvent;
}  // namespace ui

namespace views {
class MdTextButton;
}

// NOTE: Avoid using this class directly. FeaturePromoController should
// be used in almost all cases.
//
// The FeaturePromoBubbleView is a special BubbleDialogDelegateView for
// in-product help which educates users about certain Chrome features in
// a deferred context.
class FeaturePromoBubbleView : public views::BubbleDialogDelegateView {
 public:
  // Disallow copy and assign.
  FeaturePromoBubbleView(const FeaturePromoBubbleView&) = delete;
  FeaturePromoBubbleView& operator=(const FeaturePromoBubbleView&) = delete;
  ~FeaturePromoBubbleView() override;

  // NOTE: Please read comment above class. This method shouldn't be
  // called in most cases.
  //
  // Creates the promo. The returned pointer is only valid until the
  // widget is destroyed. It must not be manually deleted by the caller.
  static FeaturePromoBubbleView* Create(
      const FeaturePromoBubbleParams& params,
      base::RepeatingClosure snooze_callback = base::RepeatingClosure(),
      base::RepeatingClosure dismiss_callback = base::RepeatingClosure());

  // Closes the promo bubble.
  void CloseBubble();

  views::Button* GetDismissButtonForTesting() const;
  views::Button* GetSnoozeButtonForTesting() const;

 private:
  FeaturePromoBubbleView(const FeaturePromoBubbleParams& params,
                         base::RepeatingClosure snooze_callback,
                         base::RepeatingClosure dismiss_callback);

  // BubbleDialogDelegateView:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  gfx::Rect GetBubbleBounds() override;
  ax::mojom::Role GetAccessibleWindowRole() override;
  base::string16 GetAccessibleWindowTitle() const override;
  void UpdateHighlightedButton(bool highlighted) override {
    // Do nothing: the anchor for promo bubbles should not highlight.
  }
  gfx::Size CalculatePreferredSize() const override;

  // Determines if this bubble can be focused. If true, it will get
  // focus on creation.
  bool focusable_ = false;

  // Determines if this bubble will be dismissed when it loses focus.
  // Only meaningful when |focusable_| is true. When |allow_focus|
  // is false, the bubble will always persist because it will never
  // get blurred.
  bool persist_on_blur_ = false;

  // Determines if this bubble has dismiss and snooze buttons.
  // If true, |focusable_| must be true for keyboard accessibility.
  bool snoozable_;

  views::MdTextButton* dismiss_button_ = nullptr;
  views::MdTextButton* snooze_button_ = nullptr;

  base::string16 accessible_name_;

  base::Optional<int> preferred_width_;

  std::unique_ptr<FeaturePromoBubbleTimeout> feature_promo_bubble_timeout_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_IN_PRODUCT_HELP_FEATURE_PROMO_BUBBLE_VIEW_H_
