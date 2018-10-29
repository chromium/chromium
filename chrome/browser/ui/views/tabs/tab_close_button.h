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
  TabCloseButton(views::ButtonListener* listener,
                 MouseEventCallback mouse_event_callback);
  ~TabCloseButton() override;

  // Returns the width of the tab close button.
  static int GetWidth();

  // This function must be called before the tab is painted so it knows what
  // colors to use. It must also be called when the background color of the tab
  // changes (this class does not track tab activation state), and when the
  // theme changes.
  void SetIconColors(SkColor icon_color,
                     SkColor hovered_icon_color,
                     SkColor pressed_icon_color,
                     SkColor hovered_color,
                     SkColor pressed_color);

  // views::View:
  View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  const char* GetClassName() const override;
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;

 protected:
  void PaintButtonContents(gfx::Canvas* canvas) override;

 private:
  // views::MaskedTargeterDelegate:
  views::View* TargetForRect(views::View* root, const gfx::Rect& rect) override;
  bool GetHitTestMask(gfx::Path* mask) const override;

  // Draw the highlight circle.
  void DrawHighlight(gfx::Canvas* canvas, ButtonState state);

  // Draw the close "X" glyph.
  void DrawCloseGlyph(gfx::Canvas* canvas, ButtonState state);

  MouseEventCallback mouse_event_callback_;

  SkColor icon_colors_[views::Button::STATE_PRESSED + 1];
  SkColor highlight_colors_[views::Button::STATE_PRESSED + 1];

  DISALLOW_COPY_AND_ASSIGN(TabCloseButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_CLOSE_BUTTON_H_
