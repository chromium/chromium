// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FEATURE_PROMOS_FEATURE_PROMO_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FEATURE_PROMOS_FEATURE_PROMO_BUBBLE_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/feature_promos/feature_promo_bubble_timeout.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace gfx {
class Rect;
}

namespace ui {
class Accelerator;
class MouseEvent;
}

// The FeaturePromoBubbleView is a special BubbleDialogDelegateView for
// in-product help which educates users about certain Chrome features in a
// deferred context.
class FeaturePromoBubbleView : public views::BubbleDialogDelegateView {
 public:
  enum class ActivationAction {
    DO_NOT_ACTIVATE,
    ACTIVATE,
  };

  ~FeaturePromoBubbleView() override;

  // Creates a promo bubble. The returned pointer is only valid until the widget
  // is closed. It must not be manually deleted by the caller.
  // * |anchor_view| is the View this bubble is anchored to.
  // * |arrow| specifies where on the border the bubble's arrow is located.
  // * |string_specifier| is a string ID that can be passed to
  // |l10n_util::GetStringUTF16()|.
  // * |screenreader_string_specifier| is an optional alternate string to be
  // exposed to screen readers.
  // * |feature_accelerator| is an optional keyboard accelerator to be announced
  // by screen readers. If |screenreader_string_specifier| is used and has a
  // placeholder, |feature_accelerator|'s shortcut text will be filled in.
  // * |activation_action| specifies whether the bubble's widget will be
  // activated.
  static FeaturePromoBubbleView* CreateOwned(
      views::View* anchor_view,
      views::BubbleBorder::Arrow arrow,
      ActivationAction activation_action,
      int string_specifier,
      base::Optional<int> screenreader_string_specifier = base::nullopt,
      base::Optional<ui::Accelerator> feature_accelerator = base::nullopt,
      std::unique_ptr<FeaturePromoBubbleTimeout> feature_promo_bubble_timeout =
          nullptr);

  // Closes the promo bubble.
  void CloseBubble();

 private:
  FeaturePromoBubbleView(
      views::View* anchor_view,
      views::BubbleBorder::Arrow arrow,
      ActivationAction activation_action,
      int string_specifier,
      base::Optional<int> screenreader_string_specifier,
      base::Optional<ui::Accelerator> feature_accelerator,
      std::unique_ptr<FeaturePromoBubbleTimeout> feature_promo_bubble_timeout);

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

  const ActivationAction activation_action_;

  base::string16 accessible_name_;

  std::unique_ptr<FeaturePromoBubbleTimeout> feature_promo_bubble_timeout_;

  DISALLOW_COPY_AND_ASSIGN(FeaturePromoBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FEATURE_PROMOS_FEATURE_PROMO_BUBBLE_VIEW_H_
