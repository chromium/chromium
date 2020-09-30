// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_CLOSE_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_CLOSE_BUTTON_H_

#include "base/callback_forward.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/masked_targeter_delegate.h"

// This is a Button subclass that shows the tab closed icon.
//
// In addition to setup for the icon, it forwards middle clicks to the parent
// View by explicitly not handling them in OnMousePressed.
class TabCloseButton : public views::ImageButton,
                       public views::MaskedTargeterDelegate {
 public:
  using MouseEventCallback =
      base::Callback<void(views::View*, const ui::MouseEvent&)>;

  // The mouse_event callback will be called for every mouse event to allow
  // middle clicks to be handled by the parent.
  //
  // See note on SetTabColor.
  TabCloseButton(PressedCallback pressed_callback,
                 MouseEventCallback mouse_event_callback);
  TabCloseButton(const TabCloseButton&) = delete;
  TabCloseButton& operator=(const TabCloseButton&) = delete;
  ~TabCloseButton() override;

  // Returns the width/height of the tab close button, sans insets/padding.
  static int GetGlyphSize();

  // This function must be called before the tab is painted so it knows what
  // colors to use. It must also be called when the background color of the tab
  // changes (this class does not track tab activation state), and when the
  // theme changes.
  void SetIconColors(SkColor foreground_color, SkColor background_color);

  // Sets the desired padding around the icon. Only the icon is a target for
  // mouse clicks, but the entire button is a target for touch events, since the
  // button itself is small. Note that this is cheaper than, for example,
  // installing a new EmptyBorder every time we want to change the padding
  // around the icon.
  void SetButtonPadding(const gfx::Insets& padding);

  // views::ImageButton:
  const char* GetClassName() const override;
  View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  gfx::Insets GetInsets() const override;

 protected:
  // views::ImageButton:
  gfx::Size CalculatePreferredSize() const override;
  void PaintButtonContents(gfx::Canvas* canvas) override;

 private:
  // views::MaskedTargeterDelegate:
  views::View* TargetForRect(views::View* root, const gfx::Rect& rect) override;
  bool GetHitTestMask(SkPath* mask) const override;

  MouseEventCallback mouse_event_callback_;

  SkColor icon_color_ = gfx::kPlaceholderColor;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_CLOSE_BUTTON_H_
