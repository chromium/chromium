// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_VIEWS_HELP_BUBBLE_VIEW_H_
#define COMPONENTS_USER_EDUCATION_VIEWS_HELP_BUBBLE_VIEW_H_

#include <cstddef>
#include <memory>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/user_education/common/help_bubble_params.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/label_button.h"

namespace ui {
class MouseEvent;
}  // namespace ui

namespace views {
class ImageView;
class Label;
class MdTextButton;
}  // namespace views

namespace user_education {

class HelpBubbleDelegate;

namespace internal {

// Describes how a help bubble should be anchored to a Views element, beyond
// what is specified by the HelpBubbleParams. Should only be instantiated by
// classes derived from HelpBubbleFactory (or in tests).
struct HelpBubbleAnchorParams {
  // This is the View to be anchored to (mandatory).
  raw_ptr<views::View> view = nullptr;

  // This is an optional override of the anchor rect in screen coordinates.
  // If unspecified, the bubble is anchored as normal to `view`.
  absl::optional<gfx::Rect> rect;

  // Whether or not a visible arrow should be shown.
  bool show_arrow = true;
};

}  // namespace internal

// The HelpBubbleView is a special BubbleDialogDelegateView for
// in-product help which educates users about certain Chrome features in
// a deferred context.
class HelpBubbleView : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(HelpBubbleView);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kHelpBubbleElementIdForTesting);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kDefaultButtonIdForTesting);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kFirstNonDefaultButtonIdForTesting);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kBodyTextIdForTesting);

  HelpBubbleView(const HelpBubbleDelegate* delegate,
                 const internal::HelpBubbleAnchorParams& anchor,
                 HelpBubbleParams params);
  HelpBubbleView(const HelpBubbleView&) = delete;
  HelpBubbleView& operator=(const HelpBubbleView&) = delete;
  ~HelpBubbleView() override;

  // Returns whether the given dialog is a help bubble.
  static bool IsHelpBubble(views::DialogDelegate* dialog);

  bool IsFocusInHelpBubble() const;

  views::LabelButton* GetDefaultButtonForTesting() const;
  views::LabelButton* GetNonDefaultButtonForTesting(int index) const;

  void SetForceAnchorRect(gfx::Rect force_anchor_rect);

 protected:
  // BubbleDialogDelegateView:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  std::u16string GetAccessibleWindowTitle() const override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnThemeChanged() override;
  gfx::Size CalculatePreferredSize() const override;
  gfx::Rect GetAnchorRect() const override;
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(HelpBubbleViewTimeoutTest,
                           RespectsProvidedTimeoutAfterActivate);
  friend class HelpBubbleViewsTest;

  void MaybeStartAutoCloseTimer();

  void OnTimeout();

  const raw_ptr<const HelpBubbleDelegate> delegate_;

  // If set, overrides the anchor bounds within the anchor view.
  absl::optional<gfx::Rect> local_anchor_bounds_;

  raw_ptr<views::ImageView> icon_view_ = nullptr;
  std::vector<views::Label*> labels_;

  // If the bubble has buttons, it must be focusable.
  std::vector<views::MdTextButton*> non_default_buttons_;
  raw_ptr<views::MdTextButton> default_button_ = nullptr;
  raw_ptr<views::Button> close_button_ = nullptr;

  // This is the base accessible name of the window.
  std::u16string accessible_name_;

  // This is any additional hint text to read.
  std::u16string screenreader_hint_text_;

  // Track the number of times the widget has been activated; if it's greater
  // than 1 we won't re-read the screenreader hint again.
  int activate_count_ = 0;

  // Prevents the widget we're anchored to from disappearing when it loses
  // focus, even if it's marked as close_on_deactivate.
  std::unique_ptr<CloseOnDeactivatePin> anchor_pin_;

  // Auto close timeout. If the value is 0 (default), the bubble never times
  // out.
  base::TimeDelta timeout_;
  base::OneShotTimer auto_close_timer_;

  base::OnceClosure timeout_callback_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_VIEWS_HELP_BUBBLE_VIEW_H_
