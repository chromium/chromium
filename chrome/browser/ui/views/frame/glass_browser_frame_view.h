// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_BROWSER_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_BROWSER_FRAME_VIEW_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/win/scoped_gdi_object.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/windows_10_caption_button.h"
#include "chrome/browser/ui/views/tab_icon_view.h"
#include "chrome/browser/ui/views/tab_icon_view_model.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/window/non_client_view.h"

class BrowserView;

class GlassBrowserFrameView : public BrowserNonClientFrameView,
                              public views::ButtonListener,
                              public TabIconViewModel {
 public:
  // Alpha to use for features in the titlebar (the window title and caption
  // buttons) when the window is inactive. They are opaque when active.
  static constexpr SkAlpha kInactiveTitlebarFeatureAlpha = 0x66;

  static constexpr char kClassName[] = "GlassBrowserFrameView";

  static SkColor GetReadableFeatureColor(SkColor background_color);

  // Constructs a non-client view for an BrowserFrame.
  GlassBrowserFrameView(BrowserFrame* frame, BrowserView* browser_view);
  ~GlassBrowserFrameView() override;

  // BrowserNonClientFrameView:
  bool CaptionButtonsOnLeadingEdge() const override;
  gfx::Rect GetBoundsForTabStripRegion(
      const views::View* tabstrip) const override;
  int GetTopInset(bool restored) const override;
  int GetThemeBackgroundXInset() const override;
  bool HasVisibleBackgroundTabShapes(
      BrowserFrameActiveState active_state) const override;
  bool CanDrawStrokes() const override;
  SkColor GetCaptionColor(BrowserFrameActiveState active_state) const override;
  void UpdateThrobber(bool running) override;
  gfx::Size GetMinimumSize() const override;

  // views::NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void UpdateWindowIcon() override;
  void UpdateWindowTitle() override;
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override {}
  void ResetWindowControls() override;
  void SizeConstraintsChanged() override {}

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // TabIconViewModel:
  bool ShouldTabIconViewAnimate() const override;
  gfx::ImageSkia GetFaviconForTabIconView() override;

  bool IsMaximized() const;

  // Visual height of the titlebar when the window is maximized (i.e. excluding
  // the area above the top of the screen).
  int TitlebarMaximizedVisualHeight() const;

  SkColor GetTitlebarColor() const;

  views::Label* window_title_for_testing() { return window_title_; }

 protected:
  // views::View:
  const char* GetClassName() const override;
  void OnPaint(gfx::Canvas* canvas) override;
  void Layout() override;

 private:
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

  // Returns the y coordinate for the top of the frame, which in maximized mode
  // is the top of the screen and in restored mode is 1 pixel below the top of
  // the window to leave room for the visual border that Windows draws.
  int WindowTopY() const;

  // Returns the distance from the leading edge of the window to the leading
  // edge of the caption buttons.
  int MinimizeButtonX() const;

  // Returns whether the toolbar is currently visible.
  bool IsToolbarVisible() const;

  bool ShowCustomIcon() const;
  bool ShowCustomTitle() const;
  bool ShowSystemIcon() const;

  Windows10CaptionButton* CreateCaptionButton(ViewID button_type,
                                              int accessible_name_resource_id);

  // Paint various sub-components of this view.
  void PaintTitlebar(gfx::Canvas* canvas) const;

  // Layout various sub-components of this view.
  void LayoutTitleBar();
  void LayoutCaptionButtons();
  void LayoutCaptionButton(Windows10CaptionButton* button,
                           int previous_button_x);
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
  TabIconView* window_icon_;
  views::Label* window_title_;

  // Custom-drawn caption buttons. Only used when custom-drawing the titlebar.
  Windows10CaptionButton* minimize_button_;
  Windows10CaptionButton* maximize_button_;
  Windows10CaptionButton* restore_button_;
  Windows10CaptionButton* close_button_;

  // Whether or not the window throbber is currently animating.
  bool throbber_running_;

  // The index of the current frame of the throbber animation.
  int throbber_frame_;

  // How much extra space to reserve in non-maximized windows for a drag handle.
  int drag_handle_padding_;

  static const int kThrobberIconCount = 24;
  static HICON throbber_icons_[kThrobberIconCount];
  static void InitThrobberIcons();

  DISALLOW_COPY_AND_ASSIGN(GlassBrowserFrameView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_BROWSER_FRAME_VIEW_H_
