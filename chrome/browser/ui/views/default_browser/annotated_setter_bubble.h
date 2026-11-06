// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DEFAULT_BROWSER_ANNOTATED_SETTER_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_DEFAULT_BROWSER_ANNOTATED_SETTER_BUBBLE_H_

#include <memory>

#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"

namespace gfx {
class Rect;
}

namespace default_browser {

// Points at the "Set default" button of the Windows Settings window. Anchored
// to a screen rect, since the button belongs to another process. Must outlive
// the widget Show() returns.
class AnnotatedSetterBubble : public views::BubbleDialogDelegate {
 public:
  // `can_pin_to_taskbar` adds the taskbar-pin step.
  explicit AnnotatedSetterBubble(bool can_pin_to_taskbar);

  AnnotatedSetterBubble(const AnnotatedSetterBubble&) = delete;
  AnnotatedSetterBubble& operator=(const AnnotatedSetterBubble&) = delete;

  ~AnnotatedSetterBubble() override;

  // Shows the bubble inactive, anchored to `button_rect_dip`. `on_closed` runs
  // when the user closes it and must destroy the widget.
  std::unique_ptr<views::Widget> Show(const gfx::Rect& button_rect_dip,
                                      views::Widget::ClosedCallback on_closed);

 private:
  // views::WidgetDelegate:
  void OnWidgetInitialized() override;
};

}  // namespace default_browser

#endif  // CHROME_BROWSER_UI_VIEWS_DEFAULT_BROWSER_ANNOTATED_SETTER_BUBBLE_H_
