// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/non_client_view.h"

class BrowserView;

// This enum is used for functions who rely on the state of the browser to alter
// the appearance of the window frame.
enum class BrowserFrameActiveState {
  // Use the window's actual current active/inactive state.
  kUseCurrent,
  // Force the frame to be treated as active, regardless of the current state.
  // Note: Only used on ChromeOS.
  kActive,
  // Force the frame to be treated as inactive, regardless of the current sate.
  // Note: Only used on ChromeOS.
  kInactive,
};

// BrowserNonClientFrameView is an abstract base class that defines the
// interface for the part of a browser window that is not the "client area"
// (where the web content is displayed). This includes the title bar, window
// borders, and caption buttons (minimize, maximize, close).
//
// This class is responsible for:
// - Laying out major UI components like the tab strip.
// - Painting the window frame, taking into account the browser theme.
// - Responding to window state changes (fullscreen, activation, maximization).
//
// Concrete implementations are provided for each platform (e.g., Windows, Mac,
// Linux) and are created by the factory function
// `chrome::CreateBrowserNonClientFrameView`.
class BrowserNonClientFrameView : public views::NonClientFrameView {
  METADATA_HEADER(BrowserNonClientFrameView, views::NonClientFrameView)

 public:
  // The minimum height of the region at the top of the frame that should be
  // draggable to move the window.
  static constexpr int kMinimumDragHeight = 8;

  BrowserNonClientFrameView(BrowserFrame* frame, BrowserView* browser_view);
  BrowserNonClientFrameView(const BrowserNonClientFrameView&) = delete;
  BrowserNonClientFrameView& operator=(const BrowserNonClientFrameView&) =
      delete;
  ~BrowserNonClientFrameView() override;

  BrowserView* browser_view() const { return browser_view_; }
  BrowserFrame* frame() const { return frame_; }

  // Called after BrowserView has initialized its child views. This is a useful
  // hook for performing final setup that depends on other child views, like
  // the tabstrip or toolbar, being present.
  virtual void OnBrowserViewInitViewsComplete();

  // Called when the browser window's fullscreen state changes.
  virtual void OnFullscreenStateChanged();

  // Returns whether the caption buttons are drawn at the leading edge (e.g. on
  // the left for LTR languages, such as on macOS).
  virtual bool CaptionButtonsOnLeadingEdge() const;

  // Returns the bounds, in this view's coordinates, that the tab
  // strip should occupy.
  virtual gfx::Rect GetBoundsForTabStripRegion(
      const gfx::Size& tabstrip_minimum_size) const = 0;

  // Returns the maximum bounds, in this view's coordinates, for
  // the WebAppFrameToolbarView, which contains controls for a web app.
  virtual gfx::Rect GetBoundsForWebAppFrameToolbar(
      const gfx::Size& toolbar_preferred_size) const = 0;

  // Returns the inset from the top of the window to the top of the client
  // view. For a tabbed browser, this is the space occupied by the tab strip.
  // For popup windows, this is the toolbar. For app windows, this is the
  // WebContents. Varies on fullscreen. If |restored| is true, this is
  // calculated for the window's restored state, regardless of its current state
  // (e.g., maximized or fullscreen).
  virtual int GetTopInset(bool restored) const = 0;

  // Updates the top UI state to be hidden or shown in fullscreen according to
  // the preference's state. Currently only used on Mac.
  virtual void UpdateFullscreenTopUI();

  // Returns true if the top UI (tabstrip, toolbar) should be hidden because the
  // browser is in fullscreen mode.
  virtual bool ShouldHideTopUIForFullscreen() const;

  // Returns whether the user is allowed to exit fullscreen on their own (some
  // special modes lock the user in fullscreen).
  virtual bool CanUserExitFullscreen() const;

  // Determines whether the top of the frame is "condensed" (i.e., has less
  // vertical space). This is typically true when the window is maximized or
  // fullscreen. If true, the top frame is just the height of a tab,
  // rather than having extra vertical space above the tabs.
  virtual bool IsFrameCondensed() const;

  // Determines if background tab shapes have a distinct appearance from the
  // frame background. This is true if the theme uses a custom tab background
  // image or if the calculated color for background tabs differs from the frame
  // color.
  virtual bool HasVisibleBackgroundTabShapes(
      BrowserFrameActiveState active_state) const;

  // Returns true if background tabs are ever visibly distinct from the frame,
  // in either the active or inactive state.
  bool EverHasVisibleBackgroundTabShapes() const;

  // Returns true if strokes (outlines/separators) should be drawn around tabs.
  // This is generally true, but false for some web apps that don't have a tab
  // strip.
  virtual bool CanDrawStrokes() const;

  // Returns the color that should be used for text and icons in the title bar
  // (e.g., the window title and caption button icons).
  virtual SkColor GetCaptionColor(BrowserFrameActiveState active_state) const;

  // Returns the primary background color of the browser frame. This is also the
  // color used for the tab strip background unless overridden by a theme.
  virtual SkColor GetFrameColor(BrowserFrameActiveState active_state) const;

  // Returns the resource ID for a custom background image if the active theme
  // provides one for the frame. This checks for images for the given active
  // state and also considers theme-related fallbacks (e.g., an inactive image
  // falling back to an active one).
  std::optional<int> GetCustomBackgroundId(
      BrowserFrameActiveState active_state) const;

  // Updates the the loading animation (throbber) for the window icon in the
  // frame. Mainly used by special browsers such as PWAs.
  virtual void UpdateThrobber(bool running) = 0;

  // Signals that the frame's minimum size may have changed. This prompts the
  // widget to update its size constraints by re-querying `GetMinimumSize()`.
  // This is typically called when child views (e.g. tab strip, toolbar,
  // bookmarks bar) change visibility.The window manager is notified of this
  // change via GetWidget()->OnSizeConstraintsChanged().
  virtual void UpdateMinimumSize();

  // Called when the Window Controls Overlay state changes, allowing the frame
  // to update the state of the caption buttons accordingly.
  virtual void WindowControlsOverlayEnabledChanged() {}

  // Returns the insets from the edge of the native window to the client view in
  // DIPs. The value is left-to-right even on RTL locales. That is,
  // insets.left() will be on the left in screen coordinates. Subclasses must
  // implement this.
  virtual gfx::Insets RestoredMirroredFrameBorderInsets() const;

  // Returns the insets from the client view to the input region. The returned
  // insets will be negative, such that view_rect.Inset(GetInputInsets()) will
  // be the input region. Subclasses must implement this.
  virtual gfx::Insets GetInputInsets() const;

  // Gets the rounded-rect clipping region for the window frame when it is
  // in its restored (non-maximized) state. Subclasses must implement this.
  virtual SkRRect GetRestoredClipRegion() const;

  // Returns the height of the translucent area at the top of the frame. Returns
  // 0 if the frame is opaque (not transparent) or in fullscreen.
  virtual int GetTranslucentTopAreaHeight() const;

#if BUILDFLAG(IS_MAC)
  // Used by TabContainerOverlayView to paint tab strip background.
  virtual void PaintThemedFrame(gfx::Canvas* canvas) {}
#endif

  // Sets the bounds of `frame_`.
  virtual void SetFrameBounds(const gfx::Rect& bounds);

 protected:
  // Called when `frame_`'s "paint as active" state has changed.
  virtual void PaintAsActiveChanged();

  // Helper function to determine if we should treat the frame as the active
  // state.
  bool ShouldPaintAsActiveForState(BrowserFrameActiveState active_state) const;

  // Returns a themed image for the frame background, if one exists.
  gfx::ImageSkia GetFrameImage(BrowserFrameActiveState active_state =
                                   BrowserFrameActiveState::kUseCurrent) const;

  // Returns a themed image for the frame overlay, if one exists.
  gfx::ImageSkia GetFrameOverlayImage(
      BrowserFrameActiveState active_state =
          BrowserFrameActiveState::kUseCurrent) const;

 private:
#if BUILDFLAG(IS_WIN)
  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // views::NonClientFrameView:
  int GetSystemMenuY() const override;
#endif  // BUILDFLAG(IS_WIN)

  // The BrowserFrame that owns this view.
  const raw_ptr<BrowserFrame, DanglingUntriaged> frame_;

  // The BrowserView hosted within `frame_`.
  const raw_ptr<BrowserView, DanglingUntriaged> browser_view_;

  // Subscription to receive notifications when the frame's PaintAsActive state
  // changes.
  base::CallbackListSubscription paint_as_active_subscription_ =
      frame_->RegisterPaintAsActiveChangedCallback(
          base::BindRepeating(&BrowserNonClientFrameView::PaintAsActiveChanged,
                              base::Unretained(this)));
};

namespace chrome {

// Factory function for creating a BrowserNonClientFrameView. Platform specific
// implementations should define this in their respective
// browser_non_client_view_factor_*.cc files.
std::unique_ptr<BrowserNonClientFrameView> CreateBrowserNonClientFrameView(
    BrowserFrame* frame,
    BrowserView* browser_view);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_H_
