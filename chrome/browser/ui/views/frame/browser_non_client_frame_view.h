// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/non_client_view.h"

class BrowserView;

// Type used for functions whose return values depend on the active state of
// the frame.
enum class BrowserFrameActiveState {
  kUseCurrent,  // Use current frame active state.
  kActive,      // Treat frame as active regardless of current state.
  kInactive,    // Treat frame as inactive regardless of current state.
};

// A specialization of the NonClientFrameView object that provides additional
// Browser-specific methods.
class BrowserNonClientFrameView : public views::NonClientFrameView,
                                  public ProfileAttributesStorage::Observer {
  METADATA_HEADER(BrowserNonClientFrameView, views::NonClientFrameView)

 public:
  // The minimum total height users should have to use as a drag handle to move
  // the window with.
  static constexpr int kMinimumDragHeight = 8;

  BrowserNonClientFrameView(BrowserFrame* frame, BrowserView* browser_view);
  BrowserNonClientFrameView(const BrowserNonClientFrameView&) = delete;
  BrowserNonClientFrameView& operator=(const BrowserNonClientFrameView&) =
      delete;
  ~BrowserNonClientFrameView() override;

  BrowserView* browser_view() const { return browser_view_; }
  BrowserFrame* frame() const { return frame_; }

  // Called when BrowserView creates all it's child views.
  virtual void OnBrowserViewInitViewsComplete();

  // Called after the browser window is fullscreened or unfullscreened.
  virtual void OnFullscreenStateChanged();

  // Returns whether the caption buttons are drawn at the leading edge (i.e. the
  // left in LTR mode, or the right in RTL mode).
  virtual bool CaptionButtonsOnLeadingEdge() const;

  // Retrieves the bounds in non-client view coordinates within which the
  // TabStrip should be laid out.
  virtual gfx::Rect GetBoundsForTabStripRegion(
      const gfx::Size& tabstrip_minimum_size) const = 0;

  // Retrieves the maximum bounds in non-client view coordinates for the
  // WebAppFrameToolbarView that contains Web App controls.
  virtual gfx::Rect GetBoundsForWebAppFrameToolbar(
      const gfx::Size& toolbar_preferred_size) const = 0;

  // Lays out the window title for a web app within the given available space.
  // Unlike the above GetBounds methods this is not just a method to return the
  // bounds the title should occupy, since different implementations might also
  // want to change other attributes of the title, such as alignment.
  virtual void LayoutWebAppWindowTitle(
      const gfx::Rect& available_space,
      views::Label& window_title_label) const = 0;

  // Returns the inset of the topmost view in the client view from the top of
  // the non-client view. The topmost view depends on the window type. The
  // topmost view is the tab strip for tabbed browser windows, the toolbar for
  // popups, the web contents for app windows and varies for fullscreen windows.
  // If |restored| is true, this is calculated as if the window was restored,
  // regardless of its current state.
  virtual int GetTopInset(bool restored) const = 0;

  // Updates the top UI state to be hidden or shown in fullscreen according to
  // the preference's state. Currently only used on Mac.
  virtual void UpdateFullscreenTopUI();

  // Returns whether the top UI should hide.
  virtual bool ShouldHideTopUIForFullscreen() const;

  // Returns whether the user is allowed to exit fullscreen on their own (some
  // special modes lock the user in fullscreen).
  virtual bool CanUserExitFullscreen() const;

  // Determines whether the top frame is condensed vertically, as when the
  // window is maximized. If true, the top frame is just the height of a tab,
  // rather than having extra vertical space above the tabs.
  virtual bool IsFrameCondensed() const;

  // Returns whether the shapes of background tabs are visible against the
  // frame, given an active state of |active|.
  virtual bool HasVisibleBackgroundTabShapes(
      BrowserFrameActiveState active_state) const;

  // Returns whether the shapes of background tabs are visible against the frame
  // for either active or inactive windows.
  bool EverHasVisibleBackgroundTabShapes() const;

  // Returns whether tab strokes can be drawn.
  virtual bool CanDrawStrokes() const;

  // Returns the color to use for text, caption buttons, and other title bar
  // elements.
  virtual SkColor GetCaptionColor(BrowserFrameActiveState active_state) const;

  // Returns the color of the browser frame, which is also the color of the
  // tabstrip background.
  virtual SkColor GetFrameColor(BrowserFrameActiveState active_state) const;

  // For non-transparent windows, returns the background tab image resource ID
  // if the image has been customized, directly or indirectly, by the theme.
  std::optional<int> GetCustomBackgroundId(
      BrowserFrameActiveState active_state) const;

  // Updates the throbber.
  virtual void UpdateThrobber(bool running) = 0;

  // Provided for platform-specific updates of minimum window size.
  virtual void UpdateMinimumSize();

  // Updates the state of the title bar when window controls overlay is enabled
  // or disabled.
  virtual void WindowControlsOverlayEnabledChanged() {}

  // views::NonClientFrameView:
  using views::NonClientFrameView::ShouldPaintAsActive;
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;

  // Returns the insets from the edge of the native window to the client view in
  // DIPs. The value is left-to-right even on RTL locales. That is,
  // insets.left() will be on the left in screen coordinates.
  virtual gfx::Insets RestoredMirroredFrameBorderInsets() const;

  // Returns the insets from the client view to the input region. The returned
  // insets will be negative, such that view_rect.Inset(GetInputInsets()) will
  // be the input region.
  virtual gfx::Insets GetInputInsets() const;

  // Gets the rounded-rect that will be used to clip the window frame when
  // drawing. The region will be as if the window was restored, and will be in
  // view coordinates.
  virtual SkRRect GetRestoredClipRegion() const;

  // Returns the height of the top frame.  This value will be 0 if the
  // compositor doesn't support translucency, if the top frame is not
  // translucent, or if the window is in full screen mode.
  virtual int GetTranslucentTopAreaHeight() const;

#if BUILDFLAG(IS_MAC)
  // Used by TabContainerOverlayView to paint tab strip background.
  virtual void PaintThemedFrame(gfx::Canvas* canvas) {}
#endif

  // Sets the bounds of `frame_`.
  virtual void SetFrameBounds(const gfx::Rect& bounds);

 protected:
  // Called when |frame_|'s "paint as active" state has changed.
  virtual void PaintAsActiveChanged();

  // Converts an ActiveState to a bool representing whether the frame should be
  // treated as active.
  bool ShouldPaintAsActive(BrowserFrameActiveState active_state) const;

  // Compute aspects of the frame needed to paint the frame background.
  gfx::ImageSkia GetFrameImage(BrowserFrameActiveState active_state =
                                   BrowserFrameActiveState::kUseCurrent) const;
  gfx::ImageSkia GetFrameOverlayImage(
      BrowserFrameActiveState active_state =
          BrowserFrameActiveState::kUseCurrent) const;

  // ProfileAttributesStorage::Observer:
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const std::u16string& profile_name) override;
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;
  void OnProfileHighResAvatarLoaded(
      const base::FilePath& profile_path) override;

 private:
#if BUILDFLAG(IS_WIN)
  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // views::NonClientFrameView:
  int GetSystemMenuY() const override;
#endif  // BUILDFLAG(IS_WIN)

  // The frame that hosts this view.
  const raw_ptr<BrowserFrame, DanglingUntriaged> frame_;

  // The BrowserView hosted within this View.
  const raw_ptr<BrowserView, DanglingUntriaged> browser_view_;

  base::CallbackListSubscription paint_as_active_subscription_ =
      frame_->RegisterPaintAsActiveChangedCallback(
          base::BindRepeating(&BrowserNonClientFrameView::PaintAsActiveChanged,
                              base::Unretained(this)));
};

namespace chrome {

// Provided by a browser_non_client_frame_view_factory_*.cc implementation
std::unique_ptr<BrowserNonClientFrameView> CreateBrowserNonClientFrameView(
    BrowserFrame* frame,
    BrowserView* browser_view);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_H_
