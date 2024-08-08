// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_OPAQUE_BROWSER_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_OPAQUE_BROWSER_FRAME_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_layout_delegate.h"
#include "chrome/browser/ui/views/tab_icon_view_model.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/window/caption_button_types.h"
#include "ui/views/window/non_client_view.h"

class BrowserView;
class CaptionButtonPlaceholderContainer;
class OpaqueBrowserFrameViewLayout;
class TabIconView;

namespace chrome {
enum class FrameButtonDisplayType;
}

namespace gfx {
struct VectorIcon;
}

namespace views {
class Button;
class FrameBackground;
class Label;
}  // namespace views

class OpaqueBrowserFrameView : public BrowserNonClientFrameView,
                               public TabIconViewModel,
                               public OpaqueBrowserFrameViewLayoutDelegate {
  METADATA_HEADER(OpaqueBrowserFrameView, BrowserNonClientFrameView)

 public:
  // Constructs a non-client view for an BrowserFrame.
  OpaqueBrowserFrameView(BrowserFrame* frame,
                         BrowserView* browser_view,
                         OpaqueBrowserFrameViewLayout* layout);
  OpaqueBrowserFrameView(const OpaqueBrowserFrameView&) = delete;
  OpaqueBrowserFrameView& operator=(const OpaqueBrowserFrameView&) = delete;
  ~OpaqueBrowserFrameView() override;

  // Creates and adds child views.  Should be called after
  // OpaqueBrowserFrameView is constructed.  This is not called from the
  // constructor because it relies on virtual method calls.
  void InitViews();

  // BrowserNonClientFrameView:
  gfx::Rect GetBoundsForTabStripRegion(
      const gfx::Size& tabstrip_minimum_size) const override;
  gfx::Rect GetBoundsForWebAppFrameToolbar(
      const gfx::Size& toolbar_preferred_size) const override;
  void LayoutWebAppWindowTitle(const gfx::Rect& available_space,
                               views::Label& window_title_label) const override;
  int GetTopInset(bool restored) const override;
  void UpdateThrobber(bool running) override;
  void WindowControlsOverlayEnabledChanged() override;
  gfx::Size GetMinimumSize() const override;
  void PaintAsActiveChanged() override;
  void OnThemeChanged() override;

  // views::NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override;
  void ResetWindowControls() override;
  void UpdateWindowIcon() override;
  void UpdateWindowTitle() override;

  // TabIconViewModel:
  bool ShouldTabIconViewAnimate() const override;
  ui::ImageModel GetFaviconForTabIconView() override;

  // OpaqueBrowserFrameViewLayoutDelegate:
  bool ShouldShowWindowIcon() const override;
  bool ShouldShowWindowTitle() const override;
  std::u16string GetWindowTitle() const override;
  int GetIconSize() const override;
  gfx::Size GetBrowserViewMinimumSize() const override;
  bool ShouldShowCaptionButtons() const override;
  bool IsRegularOrGuestSession() const override;
  bool CanMaximize() const override;
  bool CanMinimize() const override;
  bool IsMaximized() const override;
  bool IsMinimized() const override;
  bool IsFullscreen() const override;
  bool IsTabStripVisible() const override;
  bool GetBorderlessModeEnabled() const override;
  int GetTabStripHeight() const override;
  bool IsToolbarVisible() const override;
  gfx::Size GetTabstripMinimumSize() const override;
  int GetTopAreaHeight() const override;
  bool UseCustomFrame() const override;
  bool IsFrameCondensed() const override;
  bool EverHasVisibleBackgroundTabShapes() const override;
  FrameButtonStyle GetFrameButtonStyle() const override;
  void UpdateWindowControlsOverlay(const gfx::Rect& bounding_rect) override;
  bool ShouldDrawRestoredFrameShadow() const override;
#if BUILDFLAG(IS_LINUX)
  bool IsTiled() const override;
#endif
  int WebAppButtonHeight() const override;

 protected:
  views::Button* minimize_button() const { return minimize_button_; }
  views::Button* maximize_button() const { return maximize_button_; }
  views::Button* restore_button() const { return restore_button_; }
  views::Button* close_button() const { return close_button_; }

  OpaqueBrowserFrameViewLayout* layout() { return layout_; }

  views::FrameBackground* frame_background() const {
    return frame_background_.get();
  }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

  // Paint various sub-components of this view.  The *FrameBorder() functions
  // also paint the background of the titlebar area, since the top frame border
  // and titlebar background are a contiguous component.
  virtual void PaintRestoredFrameBorder(gfx::Canvas* canvas) const;
  void PaintMaximizedFrameBorder(gfx::Canvas* canvas) const;
  void PaintClientEdge(gfx::Canvas* canvas) const;

 private:
  friend class WebAppOpaqueBrowserFrameViewTest;
  friend class WebAppOpaqueBrowserFrameViewWindowControlsOverlayTest;

  // Creates and returns a FrameCaptionButton with |this| as its listener.
  // Memory is owned by the caller.
  views::Button* CreateFrameCaptionButton(views::CaptionButtonIcon icon_type,
                                          int ht_component,
                                          const gfx::VectorIcon& icon_image);

  // Creates and returns an ImageButton with |this| as its listener.
  // Memory is owned by the caller.
  views::Button* CreateImageButton(int normal_image_id,
                                   int hot_image_id,
                                   int pushed_image_id,
                                   int mask_image_id,
                                   ViewID view_id);

  // Initializes state on |button| common to both FrameCaptionButtons and
  // ImageButtons.
  void InitWindowCaptionButton(views::Button* button,
                               views::Button::PressedCallback callback,
                               int accessibility_string_id,
                               ViewID view_id);

  // Returns the size of the custom image specified by |image_id| in the frame's
  // ThemeProvider.
  gfx::Size GetThemeImageSize(int image_id);

  // Returns the amount by which the background image of a caption button
  // (specified by |view_id|) should be offset on the X-axis.
  int CalculateCaptionButtonBackgroundXOffset(ViewID view_id);

  // Returns an image to be used as the background image for the caption button
  // specified by |view_id|.  The returned image is based on the control button
  // background image specified by the current theme, and processed to handle
  // size, source offset, tiling, and mirroring for the specified caption
  // button.  This is done to provide the effect that the background image
  // appears to draw contiguously across all 3 caption buttons.
  gfx::ImageSkia GetProcessedBackgroundImageForCaptionButon(
      ViewID view_id,
      const gfx::Size& desired_size);

  // Returns the insets from the native window edge to the client view.
  // This does not include any client edge.  If |restored| is true, this is
  // calculated as if the window was restored, regardless of its current
  // node_data.
  gfx::Insets FrameBorderInsets(bool restored) const;

  // Returns the thickness of the border that makes up the window frame edge
  // along the top of the frame. If |restored| is true, this acts as if the
  // window is restored regardless of the actual mode.
  int FrameTopBorderThickness(bool restored) const;

  // Returns the bounds of the titlebar icon (or where the icon would be if
  // there was one).
  gfx::Rect GetIconBounds() const;

  void WindowIconPressed();

  // Returns true if the view should draw its own custom title bar.
  bool GetShowWindowTitleBar() const;

  void UpdateCaptionButtonPlaceholderContainerBackground();

#if BUILDFLAG(IS_WIN)
  // Sets caption button's accessible name as its tooltip when it's in a PWA
  // with window-controls-overlay display override and resets it otherwise. In
  // this mode, the web contents covers the frame view and so does it's legacy
  // hwnd which prevent tooltips being shown for the caption buttons. This hwnd
  // only exists in windows.
  void UpdateCaptionButtonToolTipsForWindowControlsOverlay();
#endif

  // Our layout manager also calculates various bounds.
  raw_ptr<OpaqueBrowserFrameViewLayout> layout_;

  // Window controls.
  raw_ptr<views::Button> minimize_button_;
  raw_ptr<views::Button> maximize_button_;
  raw_ptr<views::Button> restore_button_;
  raw_ptr<views::Button> close_button_;

  // The window icon and title.
  raw_ptr<TabIconView> window_icon_;
  raw_ptr<views::Label> window_title_;

  // Background painter for the window frame.
  std::unique_ptr<views::FrameBackground> frame_background_;

#if BUILDFLAG(IS_LINUX)
  std::unique_ptr<views::MenuRunner> menu_runner_;
#endif

  // PlaceholderContainer beneath the controls button for PWAs with window
  // controls overlay display override.
  raw_ptr<CaptionButtonPlaceholderContainer, DanglingUntriaged>
      caption_button_placeholder_container_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_OPAQUE_BROWSER_FRAME_VIEW_H_
