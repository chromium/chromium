// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_OPAQUE_BROWSER_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_OPAQUE_BROWSER_FRAME_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_layout_delegate.h"
#include "chrome/browser/ui/views/tab_icon_view_model.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "ui/views/window/caption_button_types.h"
#include "ui/views/window/non_client_view.h"

class BrowserView;
class OpaqueBrowserFrameViewLayout;
class OpaqueBrowserFrameViewPlatformSpecific;
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
}

class OpaqueBrowserFrameView : public BrowserNonClientFrameView,
                               public views::ButtonListener,
                               public TabIconViewModel,
                               public OpaqueBrowserFrameViewLayoutDelegate {
 public:
  static const char kClassName[];

  // Constructs a non-client view for an BrowserFrame.
  OpaqueBrowserFrameView(BrowserFrame* frame,
                         BrowserView* browser_view,
                         OpaqueBrowserFrameViewLayout* layout);
  ~OpaqueBrowserFrameView() override;

  // Creates and adds child views.  Should be called after
  // OpaqueBrowserFrameView is constructed.  This is not called from the
  // constructor because it relies on virtual method calls.
  void InitViews();

  // BrowserNonClientFrameView:
  gfx::Rect GetBoundsForTabStripRegion(
      const views::View* tabstrip) const override;
  int GetTopInset(bool restored) const override;
  int GetThemeBackgroundXInset() const override;
  void UpdateThrobber(bool running) override;
  gfx::Size GetMinimumSize() const override;

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
  const char* GetClassName() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // TabIconViewModel:
  bool ShouldTabIconViewAnimate() const override;
  gfx::ImageSkia GetFaviconForTabIconView() override;

  // OpaqueBrowserFrameViewLayoutDelegate:
  bool ShouldShowWindowIcon() const override;
  bool ShouldShowWindowTitle() const override;
  base::string16 GetWindowTitle() const override;
  int GetIconSize() const override;
  gfx::Size GetBrowserViewMinimumSize() const override;
  bool ShouldShowCaptionButtons() const override;
  bool IsRegularOrGuestSession() const override;
  bool IsMaximized() const override;
  bool IsMinimized() const override;
  bool IsTabStripVisible() const override;
  int GetTabStripHeight() const override;
  bool IsToolbarVisible() const override;
  gfx::Size GetTabstripPreferredSize() const override;
  int GetTopAreaHeight() const override;
  bool UseCustomFrame() const override;
  bool IsFrameCondensed() const override;
  bool EverHasVisibleBackgroundTabShapes() const override;
  FrameButtonStyle GetFrameButtonStyle() const override;

 protected:
  views::Button* minimize_button() const { return minimize_button_; }
  views::Button* maximize_button() const { return maximize_button_; }
  views::Button* restore_button() const { return restore_button_; }
  views::Button* close_button() const { return close_button_; }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

  OpaqueBrowserFrameViewLayout* layout() { return layout_; }

 private:
  friend class WebAppOpaqueBrowserFrameViewTest;

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

  // Returns the thickness of the border that makes up the window frame edges.
  // This does not include any client edge.  If |restored| is true, this is
  // calculated as if the window was restored, regardless of its current
  // node_data.
  int FrameBorderThickness(bool restored) const;

  // Returns the thickness of the border that makes up the window frame edge
  // along the top of the frame. If |restored| is true, this acts as if the
  // window is restored regardless of the actual mode.
  int FrameTopBorderThickness(bool restored) const;

  // Returns the bounds of the titlebar icon (or where the icon would be if
  // there was one).
  gfx::Rect IconBounds() const;

  // Returns true if the view should draw its own custom title bar.
  bool ShouldShowWindowTitleBar() const;

  // Paint various sub-components of this view.  The *FrameBorder() functions
  // also paint the background of the titlebar area, since the top frame border
  // and titlebar background are a contiguous component.
  void PaintRestoredFrameBorder(gfx::Canvas* canvas) const;
  void PaintMaximizedFrameBorder(gfx::Canvas* canvas) const;
  void PaintClientEdge(gfx::Canvas* canvas) const;

  // Our layout manager also calculates various bounds.
  OpaqueBrowserFrameViewLayout* layout_;

  // Window controls.
  views::Button* minimize_button_;
  views::Button* maximize_button_;
  views::Button* restore_button_;
  views::Button* close_button_;

  // The window icon and title.
  TabIconView* window_icon_;
  views::Label* window_title_;

  // Background painter for the window frame.
  std::unique_ptr<views::FrameBackground> frame_background_;

  // Observer that handles platform dependent configuration.
  std::unique_ptr<OpaqueBrowserFrameViewPlatformSpecific> platform_observer_;

  DISALLOW_COPY_AND_ASSIGN(OpaqueBrowserFrameView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_OPAQUE_BROWSER_FRAME_VIEW_H_
