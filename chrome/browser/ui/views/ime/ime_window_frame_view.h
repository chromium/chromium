// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_IME_IME_WINDOW_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_IME_IME_WINDOW_FRAME_VIEW_H_

#include "chrome/browser/ui/input_method/ime_window.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/window/non_client_view.h"

namespace views {
class ImageButton;
class ImageView;
}

namespace ui {

class ImeWindowView;

// The non-client frame view implementation for the IME window UI.
class ImeWindowFrameView : public views::NonClientFrameView,
                           public views::ButtonListener {
 public:
  // According to the UX spec, the follow-cursor window needs to have the title
  // bar on the side instead of on the top (because the follow-cursor window is
  // majorly used as suggestion list which can be shown in horizontal).
  // TODO(shuchen): locate the title bar on the right in the RTL case.
  ImeWindowFrameView(ImeWindowView* ime_window_view,
                     ImeWindow::Mode mode);
  ~ImeWindowFrameView() override;

  void Init();
  void UpdateIcon();

 private:
  static constexpr int kImeBorderThickness = 1;
  static constexpr int kTitlebarHeight = 32;

  // Colors used to draw border, titlebar background and title text.
  static constexpr SkColor kImeBackgroundColor =
      SkColorSetRGB(0xec, 0xef, 0xf1);
  static constexpr SkColor kBorderColor = SkColorSetRGB(0xda, 0xdf, 0xe1);

  // views::NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override;
  void ResetWindowControls() override;
  void UpdateWindowIcon() override;
  void UpdateWindowTitle() override;
  void SizeConstraintsChanged() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void OnPaint(gfx::Canvas* canvas) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Update control styles to indicate if the titlebar is active or not.
  void UpdateControlStyles();

  // Custom draw the frame.
  void PaintFrameBackground(gfx::Canvas* canvas);

  bool in_follow_cursor_mode() const {
    return mode_ == ImeWindow::FOLLOW_CURSOR;
  }

  ImeWindowView* const ime_window_view_;
  const ImeWindow::Mode mode_;
  views::ImageButton* close_button_ = nullptr;
  views::ImageView* title_icon_ = nullptr;
  views::View* content_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ImeWindowFrameView);
};

}  // namespace ui

#endif  // CHROME_BROWSER_UI_VIEWS_IME_IME_WINDOW_FRAME_VIEW_H_
