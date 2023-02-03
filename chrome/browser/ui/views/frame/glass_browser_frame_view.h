// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_BROWSER_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_BROWSER_FRAME_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/win/scoped_gdi_object.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/windows_caption_button.h"
#include "chrome/browser/ui/views/tab_icon_view.h"
#include "chrome/browser/ui/views/tab_icon_view_model.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/non_client_view.h"

class BrowserView;
class TabSearchBubbleHost;
class GlassBrowserCaptionButtonContainer;

class GlassBrowserFrameView : public BrowserNonClientFrameView,
                              public TabIconViewModel {
 public:
  METADATA_HEADER(GlassBrowserFrameView);

  // Constructs a non-client view for an BrowserFrame.
  GlassBrowserFrameView(BrowserFrame* frame, BrowserView* browser_view);
  GlassBrowserFrameView(const GlassBrowserFrameView&) = delete;
  GlassBrowserFrameView& operator=(const GlassBrowserFrameView&) = delete;
  ~GlassBrowserFrameView() override;

  // BrowserNonClientFrameView:
  bool CaptionButtonsOnLeadingEdge() const override;
  gfx::Rect GetBoundsForTabStripRegion(
      const gfx::Size& tabstrip_minimum_size) const override;
  gfx::Rect GetBoundsForWebAppFrameToolbar(
      const gfx::Size& toolbar_preferred_size) const override;
  void LayoutWebAppWindowTitle(const gfx::Rect& available_space,
                               views::Label& window_title_label) const override;
  int GetTopInset(bool restored) const override;
  int GetThemeBackgroundXInset() const override;
  bool HasVisibleBackgroundTabShapes(
      BrowserFrameActiveState active_state) const override;
  SkColor GetCaptionColor(BrowserFrameActiveState active_state) const override;
  void UpdateThrobber(bool running) override;
  gfx::Size GetMinimumSize() const override;
  void WindowControlsOverlayEnabledChanged() override;
  TabSearchBubbleHost* GetTabSearchBubbleHost() override;

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

  // TabIconViewModel:
  bool ShouldTabIconViewAnimate() const override;
  ui::ImageModel GetFaviconForTabIconView() override;

  bool IsMaximized() const;
  bool IsWebUITabStrip() const;

  // Visual height of the titlebar when the window is maximized (i.e. excluding
  // the area above the top of the screen).
  int TitlebarMaximizedVisualHeight() const;

  SkColor GetTitlebarColor() const;

  const GlassBrowserCaptionButtonContainer*
  caption_button_container_for_testing() const {
    return caption_button_container_;
  }

 protected:
  // BrowserNonClientFrameView:
  void PaintAsActiveChanged() override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void Layout() override;

 private:
  friend class GlassBrowserCaptionButtonContainer;

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

  // Returns the y coordinate for the top of the frame, which in maximized mode
  // is the top of the screen and in restored mode is 1 pixel below the top of
  // the window to leave room for the visual border that Windows draws.
  int WindowTopY() const;

  // Returns the distance from the leading edge of the window to the leading
  // edge of the caption buttons.
  int MinimizeButtonX() const;

  // Returns whether or not the window should display an icon of the specified
  // |type|.
  bool ShouldShowWindowIcon(TitlebarType type) const;

  // Returns whether or not the window should display a title of the specified
  // |type|.
  bool ShouldShowWindowTitle(TitlebarType type) const;

  // Paint various sub-components of this view.
  void PaintTitlebar(gfx::Canvas* canvas) const;

  // Layout various sub-components of this view.
  void LayoutTitleBar();
  void LayoutCaptionButtons();
  void LayoutWindowControlsOverlay();
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
  TabIconView* window_icon_ = nullptr;
  raw_ptr<views::Label> window_title_ = nullptr;

  // The container holding the caption buttons (minimize, maximize, close, etc.)
  // May be null if the caption button container is destroyed before the frame
  // view. Always check for validity before using!
  raw_ptr<GlassBrowserCaptionButtonContainer> caption_button_container_;

  // Whether or not the window throbber is currently animating.
  bool throbber_running_ = false;

  // The index of the current frame of the throbber animation.
  int throbber_frame_ = 0;

  static const int kThrobberIconCount = 24;
  static HICON throbber_icons_[kThrobberIconCount];
  static void InitThrobberIcons();
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_BROWSER_FRAME_VIEW_H_
