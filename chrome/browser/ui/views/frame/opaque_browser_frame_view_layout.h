// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_OPAQUE_BROWSER_FRAME_VIEW_LAYOUT_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_OPAQUE_BROWSER_FRAME_VIEW_LAYOUT_H_

#include <vector>

#include "base/macros.h"
#include "chrome/browser/ui/frame_button_display_types.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/window/frame_buttons.h"

class WebAppFrameToolbarView;
class OpaqueBrowserFrameViewLayoutDelegate;

namespace views {
class Button;
class Label;
}

// Calculates the position of the widgets in the opaque browser frame view.
//
// This is separated out for testing reasons. OpaqueBrowserFrameView has tight
// dependencies with Browser and classes that depend on Browser.
class OpaqueBrowserFrameViewLayout : public views::LayoutManager {
 public:
  // Constants used by OpaqueBrowserFrameView as well.
  static const int kContentEdgeShadowThickness;

  // Constants public for testing only.
  static constexpr int kNonClientExtraTopThickness = 1;
  static const int kFrameBorderThickness;
  static const int kTopFrameEdgeThickness;
  static const int kSideFrameEdgeThickness;
  static const int kIconLeftSpacing;
  static const int kIconTitleSpacing;
  static const int kCaptionSpacing;
  static const int kCaptionButtonBottomPadding;

  OpaqueBrowserFrameViewLayout();
  ~OpaqueBrowserFrameViewLayout() override;

  void set_delegate(OpaqueBrowserFrameViewLayoutDelegate* delegate) {
    delegate_ = delegate;
  }

  // Configures the button ordering in the frame.
  void SetButtonOrdering(
      const std::vector<views::FrameButton>& leading_buttons,
      const std::vector<views::FrameButton>& trailing_buttons);

  gfx::Rect GetBoundsForTabStripRegion(const gfx::Size& tabstrip_preferred_size,
                                       int total_width) const;

  // Returns the bounds of the window required to display the content area at
  // the specified bounds.
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const;

  // Returns the thickness of the border that makes up the window frame edges.
  // This does not include any client edge.  If |restored| is true, acts as if
  // the window is restored regardless of the real mode.
  int FrameBorderThickness(bool restored) const;

  // Returns the thickness of the border that makes up the window frame edge
  // along the top of the frame. If |restored| is true, this acts as if the
  // window is restored regardless of the actual mode.
  int FrameTopBorderThickness(bool restored) const;

  // Returns the height of the entire nonclient top border, from the edge of the
  // window to the top of the tabs. If |restored| is true, this is calculated as
  // if the window was restored, regardless of its current state.
  int NonClientTopHeight(bool restored) const;

  int GetTabStripInsetsTop(bool restored) const;

  // Returns the y-coordinate of the caption button when native frame buttons
  // are disabled.  If |restored| is true, acts as if the window is restored
  // regardless of the real mode.
  int DefaultCaptionButtonY(bool restored) const;

  // Returns the y-coordinate of button |button_id|.  If |restored| is true,
  // acts as if the window is restored regardless of the real mode.
  virtual int CaptionButtonY(chrome::FrameButtonDisplayType button_id,
                             bool restored) const;

  // Returns the thickness of the top 3D edge of the window frame.  If
  // |restored| is true, acts as if the window is restored regardless of the
  // real mode.
  int FrameTopThickness(bool restored) const;

  // Returns the thickness of the left and right 3D edges of the window frame.
  // If |restored| is true, acts as if the window is restored regardless of the
  // real mode.
  int FrameSideThickness(bool restored) const;

  // Returns the bounds of the titlebar icon (or where the icon would be if
  // there was one).
  gfx::Rect IconBounds() const;

  // Returns the bounds of the client area for the specified view size.
  gfx::Rect CalculateClientAreaBounds(int width, int height) const;

  // Converts a FrameButton to a FrameButtonDisplayType, taking into
  // consideration the maximized state of the browser window.
  chrome::FrameButtonDisplayType GetButtonDisplayType(
      views::FrameButton button_id) const;

  // Returns the margin around button |button_id|.  If |leading_spacing| is
  // true, returns the left margin (in RTL), otherwise returns the right margin
  // (in RTL).  Extra margin may be added if |is_leading_button| is true.
  virtual int GetWindowCaptionSpacing(views::FrameButton button_id,
                                      bool leading_spacing,
                                      bool is_leading_button) const;

  void set_forced_window_caption_spacing_for_test(
      int forced_window_caption_spacing) {
    forced_window_caption_spacing_ = forced_window_caption_spacing;
  }

  const gfx::Rect& client_view_bounds() const { return client_view_bounds_; }

  // Returns the extra thickness of the area above the tabs.
  int GetNonClientRestoredExtraThickness() const;

  // views::LayoutManager:
  // Called explicitly from OpaqueBrowserFrameView so we can't group it with
  // the other overrides.
  gfx::Size GetMinimumSize(const views::View* host) const override;

 protected:
  // Whether a specific button should be inserted on the leading or trailing
  // side.
  enum ButtonAlignment {
    ALIGN_LEADING,
    ALIGN_TRAILING
  };

  struct TopAreaPadding {
    int leading;
    int trailing;
  };

  // views::LayoutManager:
  void Layout(views::View* host) override;

  // Returns the spacing between the edge of the browser window and the first
  // frame buttons.
  virtual TopAreaPadding GetTopAreaPadding(bool has_leading_buttons,
                                           bool has_trailing_buttons) const;

  OpaqueBrowserFrameViewLayoutDelegate* delegate_;

  // The leading and trailing x positions of the empty space available for
  // laying out titlebar elements.
  int available_space_leading_x_;
  int available_space_trailing_x_;

  // The size of the window buttons. This does not count labels or other
  // elements that should be counted in a minimal frame.
  int minimum_size_for_buttons_;

 private:
  // Layout various sub-components of this view.
  void LayoutWindowControls();
  void LayoutTitleBar();

  void ConfigureButton(views::FrameButton button_id, ButtonAlignment align);

  // Sets the visibility of all buttons associated with |button_id| to false.
  void HideButton(views::FrameButton button_id);

  // Adds a window caption button to either the leading or trailing side.
  void SetBoundsForButton(views::FrameButton button_id,
                          views::Button* button,
                          ButtonAlignment align);

  // Internal implementation of ViewAdded() and ViewRemoved().
  void SetView(int id, views::View* view);

  // Returns the spacing between the edge of the browser window and the first
  // frame buttons.
  TopAreaPadding GetTopAreaPadding() const;

  // Returns true if a 3D edge should be drawn around the window frame.  If
  // |restored| is true, acts as if the window is restored regardless of the
  // real mode.
  bool IsFrameEdgeVisible(bool restored) const;

  // views::LayoutManager:
  gfx::Size GetPreferredSize(const views::View* host) const override;
  void ViewAdded(views::View* host, views::View* view) override;
  void ViewRemoved(views::View* host, views::View* view) override;

  // The bounds of the ClientView.
  gfx::Rect client_view_bounds_;

  // The layout of the window icon, if visible.
  gfx::Rect window_icon_bounds_;

  // Whether any of the window control buttons were packed on the leading or
  // trailing sides.  This state is only valid while Layout() is being run.
  bool placed_leading_button_;
  bool placed_trailing_button_;

  // Extra offset between the individual window caption buttons.  Set only in
  // testing, otherwise, its value will be -1.
  int forced_window_caption_spacing_;

  // Window controls.
  views::Button* minimize_button_;
  views::Button* maximize_button_;
  views::Button* restore_button_;
  views::Button* close_button_;

  views::View* window_icon_;
  views::Label* window_title_;

  WebAppFrameToolbarView* web_app_frame_toolbar_ = nullptr;

  std::vector<views::FrameButton> leading_buttons_;
  std::vector<views::FrameButton> trailing_buttons_;

  DISALLOW_COPY_AND_ASSIGN(OpaqueBrowserFrameViewLayout);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_OPAQUE_BROWSER_FRAME_VIEW_LAYOUT_H_
