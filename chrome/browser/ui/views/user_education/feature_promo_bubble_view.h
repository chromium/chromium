// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_BUBBLE_VIEW_H_

#include <cstddef>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ui {
class MouseEvent;
}  // namespace ui

namespace views {
class MdTextButton;
}  // namespace views

// NOTE: Avoid using this class directly. FeaturePromoController should
// be used in almost all cases.
//
// The FeaturePromoBubbleView is a special BubbleDialogDelegateView for
// in-product help which educates users about certain Chrome features in
// a deferred context.
class FeaturePromoBubbleView : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(FeaturePromoBubbleView);
  FeaturePromoBubbleView(const FeaturePromoBubbleView&) = delete;
  FeaturePromoBubbleView& operator=(const FeaturePromoBubbleView&) = delete;
  ~FeaturePromoBubbleView() override;

  struct ButtonParams {
    ButtonParams();
    ButtonParams(ButtonParams&&);
    ~ButtonParams();

    ButtonParams& operator=(ButtonParams&&);

    std::u16string text;
    bool has_border;
    base::RepeatingClosure callback;
  };

  struct CreateParams {
    CreateParams();
    CreateParams(CreateParams&&);
    ~CreateParams();

    CreateParams& operator=(CreateParams&&);

    raw_ptr<views::View> anchor_view = nullptr;
    views::BubbleBorder::Arrow arrow = views::BubbleBorder::TOP_LEFT;

    raw_ptr<const gfx::VectorIcon> body_icon = nullptr;
    std::u16string body_text;
    std::u16string title_text;
    std::u16string screenreader_text;

    // Additional message to be read to screen reader users to aid in
    // navigation.
    std::u16string keyboard_navigation_hint;

    std::vector<ButtonParams> buttons;

    absl::optional<int> preferred_width;

    bool focus_on_create = false;
    bool persist_on_blur = true;
    bool has_close_button = false;

    // Determines how progress indicators for tutorials will be rendered. If not
    // provided, no progress indicator will be visible.
    absl::optional<int> tutorial_progress_current;
    absl::optional<int> tutorial_progress_max;

    // Sets the bubble timeout. If a timeout is not provided a default will
    // be used. If the timeout is 0, the bubble never times out.
    absl::optional<base::TimeDelta> timeout;

    // Used to call feature specific logic on dismiss.
    // (TODO) dpenning: move to using a OnceClosure.
    base::RepeatingClosure dismiss_callback;

    // Used to call feature specific logic on timeout.
    // (TODO) dpenning: move to using a OnceClosure.
    base::RepeatingClosure timeout_callback;
  };

  // NOTE: Please read comment above class. This method shouldn't be
  // called in most cases.
  //
  // Creates the promo. The returned pointer is only valid until the
  // widget is destroyed. It must not be manually deleted by the caller.
  static FeaturePromoBubbleView* Create(CreateParams params);

  // Closes the promo bubble.
  void CloseBubble();

  views::Button* GetButtonForTesting(int index) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(FeaturePromoBubbleViewTest,
                           RespectsProvidedTimeoutAfterActivate);
  explicit FeaturePromoBubbleView(CreateParams params);

  void MaybeStartAutoCloseTimer();

  void OnTimeout();

  // BubbleDialogDelegateView:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  std::u16string GetAccessibleWindowTitle() const override;
  void UpdateHighlightedButton(bool highlighted) override {
    // Do nothing: the anchor for promo bubbles should not highlight.
  }
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  gfx::Size CalculatePreferredSize() const override;

  // If the bubble has buttons, it must be focusable.
  std::vector<views::MdTextButton*> buttons_;

  // This is the base accessible name of the window.
  std::u16string accessible_name_;

  // This is any additional hint text to read.
  std::u16string screenreader_hint_text_;

  // Track the number of times the widget has been activated; if it's greater
  // than 1 we won't re-read the screenreader hint again.
  int activate_count_ = 0;

  absl::optional<int> preferred_width_;

  // Auto close timeout. If the value is 0 (default), the bubble never times
  // out.
  base::TimeDelta timeout_;
  base::OneShotTimer auto_close_timer_;

  // (TODO) dpenning: move to using a OnceClosure.
  base::RepeatingClosure timeout_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_BUBBLE_VIEW_H_
