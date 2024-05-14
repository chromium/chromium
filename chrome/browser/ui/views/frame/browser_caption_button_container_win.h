// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_CAPTION_BUTTON_CONTAINER_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_CAPTION_BUTTON_CONTAINER_WIN_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class BrowserFrameViewWin;
class WindowsCaptionButton;

// Provides a container for Windows caption buttons that can be moved between
// frame and browser window as needed. When extended horizontally, becomes a
// grab bar for moving the window.
class BrowserCaptionButtonContainer : public views::View,
                                      public views::WidgetObserver {
  METADATA_HEADER(BrowserCaptionButtonContainer, views::View)

 public:
  explicit BrowserCaptionButtonContainer(BrowserFrameViewWin* frame_view);
  ~BrowserCaptionButtonContainer() override;

  // Tests to see if the specified |point| (which is expressed in this view's
  // coordinates and which must be within this view's bounds) is within one of
  // the caption buttons. Returns one of HitTestCompat enum defined in
  // ui/base/hit_test.h, HTCAPTION if the area hit would be part of the window's
  // drag handle, and HTNOWHERE otherwise.
  // See also ClientView::NonClientHitTest.
  int NonClientHitTest(const gfx::Point& point) const;

  void OnWindowControlsOverlayEnabledChanged();

 private:
  friend class BrowserFrameViewWin;

  // views::View:
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void OnThemeChanged() override;

  // views::WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

  void ResetWindowControls();

  // Sets caption button visibility and enabled state based on window state.
  // Only one of maximize or restore button should ever be visible at the same
  // time, and both are disabled in tablet UI mode.
  void UpdateButtons();

  // Sets caption button's accessible name as its tooltip when it's in a PWA
  // with window-controls-overlay display override and resets it otherwise. In
  // this mode, the web contents covers the frame view and so does it's legacy
  // hwnd which prevent tooltips being shown for the caption buttons.
  void UpdateButtonToolTipsForWindowControlsOverlay();

  const raw_ptr<BrowserFrameViewWin> frame_view_;
  const raw_ptr<WindowsCaptionButton> minimize_button_;
  const raw_ptr<WindowsCaptionButton> maximize_button_;
  const raw_ptr<WindowsCaptionButton> restore_button_;
  const raw_ptr<WindowsCaptionButton> close_button_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  base::CallbackListSubscription subscription_ =
      ui::TouchUiController::Get()->RegisterCallback(
          base::BindRepeating(&BrowserCaptionButtonContainer::UpdateButtons,
                              base::Unretained(this)));
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_CAPTION_BUTTON_CONTAINER_WIN_H_
