// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_WIN_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/win/scoped_gdi_object.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/windows_caption_button.h"
#include "chrome/browser/ui/views/tab_icon_view.h"
#include "chrome/browser/ui/views/tab_icon_view_model.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/views/window/non_client_view.h"

class BrowserView;
class BrowserCaptionButtonContainer;

class BrowserFrameViewWin : public BrowserNonClientFrameView,
                            public TabIconViewModel {
  METADATA_HEADER(BrowserFrameViewWin, BrowserNonClientFrameView)

 public:
  // Constructs a non-client view for an BrowserFrame.
  BrowserFrameViewWin(BrowserFrame* frame, BrowserView* browser_view);
  BrowserFrameViewWin(const BrowserFrameViewWin&) = delete;
  BrowserFrameViewWin& operator=(const BrowserFrameViewWin&) = delete;
  ~BrowserFrameViewWin() override;

  // BrowserNonClientFrameView:
  bool CaptionButtonsOnLeadingEdge() const override;
  gfx::Rect GetBoundsForTabStripRegion(
      const gfx::Size& tabstrip_minimum_size) const override;
  gfx::Rect GetBoundsForWebAppFrameToolbar(
      const gfx::Size& toolbar_preferred_size) const override;
  void LayoutWebAppWindowTitle(const gfx::Rect& available_space,
                               views::Label& window_title_label) const override;
  int GetTopInset(bool restored) const override;
  bool HasVisibleBackgroundTabShapes(
      BrowserFrameActiveState active_state) const override;
  SkColor GetCaptionColor(BrowserFrameActiveState active_state) const override;
  void UpdateThrobber(bool running) override;
  gfx::Size GetMinimumSize() const override;
  void WindowControlsOverlayEnabledChanged() override;

  // views::NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void UpdateWindowIcon() override;
  void UpdateWindowTitle() override;
  void ResetWindowControls() override;
  void OnThemeChanged() override;

  // TabIconViewModel:
  bool ShouldTabIconViewAnimate() const override;
  ui::ImageModel GetFaviconForTabIconView() override;

  bool IsMaximized() const;
  bool IsWebUITabStrip() const;

  // Returns the y coordinate for the top of the frame, which in maximized mode
  // is the top of the screen and in restored mode is 1 pixel below the top of
  // the window to leave room for the visual border that Windows draws.
  int WindowTopY() const;

  // Visual height of the titlebar when the window is maximized (i.e. excluding
  // the area above the top of the screen).
  int TitlebarMaximizedVisualHeight() const;

  SkColor GetTitlebarColor() const;

  const BrowserCaptionButtonContainer* caption_button_container_for_testing()
      const {
    return caption_button_container_;
  }

  const TabIconView* window_icon_for_testing() const { return window_icon_; }

 protected:
  // BrowserNonClientFrameView:
  void PaintAsActiveChanged() override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void Layout(PassKey) override;

 private:
  friend class BrowserCaptionButtonContainer;

  // Describes the type of titlebar that a window might have; used to query
  // whether specific elements may be present.
  enum class TitlebarType {
    // A custom drawn titlebar, with window title and/or icon.
    kCustom,
    // The system titlebar, drawn by Windows.
    kSystem,
    // Any visible titlebar.
    kAny
  };

  // Returns the thickness of the window border for the left, right, and bottom
  // edges of the frame. On Windows 10 this is a mostly-transparent handle that
  // allows you to resize the window.
  int FrameBorderThickness() const;

  // Returns the thickness of the window border for the top edge of the frame,
  // which is sometimes different than FrameBorderThickness(). Does not include
  // the titlebar/tabstrip area. If |restored| is true, this is calculated as if
  // the window was restored, regardless of its current state.
  int FrameTopBorderThickness(bool restored) const;
  int FrameTopBorderThicknessPx(bool restored) const;

  // Returns the height of everything above the tabstrip's hit-test region,
  // including both the window border (i.e. FrameTopBorderThickness()) and any
  // additional draggable area that's considered part of the window frame rather
  // than the tabstrip. If |restored| is true, this is calculated as if the
  // window was restored, regardless of its current state.
  int TopAreaHeight(bool restored) const;

  // Returns the height of the titlebar for popups or other browser types that
  // don't have tabs.
  int TitlebarHeight(bool restored) const;

  // Returns the height of the frame, whether that is a tabstrip or a titlebar.
  int GetFrameHeight() const;

  // Returns the width of the caption buttons region, including visible
  // system-drawn and custom-drawn caption buttons.
  int CaptionButtonsRegionWidth() const;

  // Returns whether or not the window should display an icon of the specified
  // |type|.
  bool ShouldShowWindowIcon(TitlebarType type) const;

  // Returns whether or not the window should display a title of the specified
  // |type|.
  bool ShouldShowWindowTitle(TitlebarType type) const;

  // Called when the device enters or exits tablet mode.
  void TabletModeChanged();

  // Sets DWM attributes for rendering the system-drawn Mica titlebar.
  void SetSystemMicaTitlebarAttributes();

  // Paint various sub-components of this view.
  void PaintTitlebar(gfx::Canvas* canvas) const;

  // Layout various sub-components of this view.
  void LayoutTitleBar();
  void LayoutCaptionButtons();
  void LayoutClientView();

  // Returns the insets of the client area. If |restored| is true, this is
  // calculated as if the window was restored, regardless of its current state.
  gfx::Insets GetClientAreaInsets(bool restored) const;

  // Starts/Stops the window throbber running.
  void StartThrobber();
  void StopThrobber();

  // Displays the next throbber frame.
  void DisplayNextThrobberFrame();

  // The bounds of the ClientView.
  gfx::Rect client_view_bounds_;

  // The small icon created from the bitmap image of the window icon.
  base::win::ScopedHICON small_window_icon_;

  // The big icon created from the bitmap image of the window icon.
  base::win::ScopedHICON big_window_icon_;

  // Icon and title. Only used when custom-drawing the titlebar for popups.
  raw_ptr<TabIconView> window_icon_ = nullptr;
  raw_ptr<views::Label> window_title_ = nullptr;

  // The container holding the caption buttons (minimize, maximize, close, etc.)
  raw_ptr<BrowserCaptionButtonContainer> caption_button_container_;

  base::CallbackListSubscription tablet_mode_subscription_ =
      ui::TouchUiController::Get()->RegisterCallback(
          base::BindRepeating(&BrowserFrameViewWin::TabletModeChanged,
                              base::Unretained(this)));

  // Whether or not the window throbber is currently animating.
  bool throbber_running_ = false;

  // The index of the current frame of the throbber animation.
  int throbber_frame_ = 0;

  static const int kThrobberIconCount = 24;
  static HICON throbber_icons_[kThrobberIconCount];
  static void InitThrobberIcons();
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_WIN_H_
