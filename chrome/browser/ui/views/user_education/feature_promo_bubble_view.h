// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_BUBBLE_VIEW_H_

#include <cstddef>
#include <memory>

#include "base/timer/timer.h"
#include "chrome/browser/ui/user_education/feature_promo_bubble_params.h"
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

    views::View* anchor_view = nullptr;
    views::BubbleBorder::Arrow arrow = views::BubbleBorder::TOP_LEFT;

    const gfx::VectorIcon* body_icon = nullptr;
    std::u16string body_text;
    absl::optional<std::u16string> title_text;
    absl::optional<std::u16string> screenreader_text;

    std::vector<ButtonParams> buttons;

    absl::optional<int> preferred_width;

    bool focus_on_create = false;
    bool persist_on_blur = true;
    bool has_close_button = false;

    // Determines how progress indicators for tutorials will be rendered. If not
    // provided, no progress indicator will be visible.
    absl::optional<int> tutorial_progress_current;
    absl::optional<int> tutorial_progress_max;

    // Changes the bubble timeout before and after hovering the bubble,
    // respectively. If a timeout is not provided a default will be used. If
    // |timeout_after_interaction| is 0, |timeout_no_interaction| is used in
    // both cases. If both are 0, the bubble never times out. A bubble with
    // buttons never times out regardless of the values.
    absl::optional<base::TimeDelta> timeout_no_interaction;
    absl::optional<base::TimeDelta> timeout_after_interaction;

    // Used to call feature specific logic on dismiss.
    absl::optional<base::RepeatingClosure> dismiss_callback;

    // Used to call feature specific logic on timeout.
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
  explicit FeaturePromoBubbleView(CreateParams params);

  void StartAutoCloseTimer(base::TimeDelta auto_close_duration);

  void OnTimeout();

  // BubbleDialogDelegateView:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  std::u16string GetAccessibleWindowTitle() const override;
  void UpdateHighlightedButton(bool highlighted) override {
    // Do nothing: the anchor for promo bubbles should not highlight.
  }
  gfx::Size CalculatePreferredSize() const override;

  // If the bubble has buttons, it must be focusable.
  std::vector<views::MdTextButton*> buttons_;

  std::u16string accessible_name_;

  absl::optional<int> preferred_width_;

  // Auto close timeouts for before and after the bubble is hovered. If the
  // latter is 0, only the former is used. If both are 0, the bubble never times
  // out.
  base::TimeDelta timeout_no_interaction_;
  base::TimeDelta timeout_after_interaction_;

  base::OneShotTimer auto_close_timer_;
  base::RepeatingClosure timeout_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_BUBBLE_VIEW_H_
