// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_FRAME_VIEW_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/content_settings/content_setting_image_view_delegate.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "components/security_state/core/security_state.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/frame_view.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class FlexLayoutView;
class ImageButton;
class Label;
class Widget;
}  // namespace views

class AutoPipSettingOverlayView;
class ContentSettingImageView;
class DocumentPipHost;
class LocationBarModelImpl;

// DocumentPipFrameView is the non-client frame view for the standalone Document
// Picture-in-Picture widget. It replaces PictureInPictureBrowserFrameView's
// role for the standalone path by inheriting from views::FrameView directly
// (not BrowserFrameView) and taking a DocumentPipHost* instead of a
// BrowserView*.
//
// Provides:
//   - A title bar showing the opener origin with a thin security/page-info
//     "chip" (a lock icon + origin label) that opens the Page Info dialog.
//   - A close button and a back-to-tab button.
//   - NonClientHitTest for proper drag, resize, and control interaction.
//
// View tree:
//
//   DocumentPipFrameView (views::FrameView)
//   |- top_bar_container_view_ (FlexLayoutView, horizontal, cross=center)
//   |  |- origin_chip_ (IconLabelBubbleView) -> opens Page Info on click
//   |  |- window_title_ (views::Label, flex: scale-to-zero..preferred)
//   |  |- spacer (views::View, flex: scale-to-zero..unbounded, draggable)
//   |  `- button_container_view_ (FlexLayoutView)
//   |     |- content_setting_views_[] (ContentSettingImageView: camera/mic)
//   |     |- back_to_tab_wrapper (views::View, fill) [optional]
//   |     |  `- back_to_tab_button_ (views::ImageButton)
//   |     `- close_wrapper (views::View, fill)
//   |        `- close_image_button_ (views::ImageButton)
//   `- auto_pip_setting_overlay_ (AutoPipSettingOverlayView) [optional,
//        overlaid on the client area, not a child of the top bar]
//
// Layout:
//
//   +-------------------------- PiP window --------------------------------+
//   | +-- top_bar_container_view_ (kTopControlsHeight = 34px) -----------+ |
//   | | [# origin_chip_] window_title_  <-spacer->  [cam][mic] [<>][x]   | |
//   | +------------------------------------------------------------------+ |
//   | +------------------------------------------------------------------+ |
//   | |                                                                  | |
//   | |                  client view (web contents)                      | |
//   | |     (auto_pip_setting_overlay_ drawn over this when present)     | |
//   | |                                                                  | |
//   | +------------------------------------------------------------------+ |
//   +----------------------------------------------------------------------+
//
// The origin chip is driven by a LocationBarModelImpl backed by a minimal
// delegate over the opener WebContents, reusing the omnibox's URL/security
// logic. Page Info is opened via the //chrome/browser/ui/views/page_info
// bubble.
class DocumentPipFrameView : public views::FrameView,
                             public views::WidgetObserver,
                             public IconLabelBubbleView::Delegate,
                             public ContentSettingImageViewDelegate {
  METADATA_HEADER(DocumentPipFrameView, views::FrameView)

 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class CloseReason {
    kOther = 0,
    kBackToTabButton = 1,
    kCloseButton = 2,
    kMaxValue = kCloseButton,
  };
  // `host` must outlive this view. The host's PiP options decide whether to
  // show the back-to-tab button. The host's opener WebContents provides the
  // origin and security state surfaced in the title bar.
  explicit DocumentPipFrameView(DocumentPipHost* host);

  DocumentPipFrameView(const DocumentPipFrameView&) = delete;
  DocumentPipFrameView& operator=(const DocumentPipFrameView&) = delete;

  ~DocumentPipFrameView() override;

  // views::FrameView:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void OnThemeChanged() override;
  void Layout(PassKey) override;
  void AddedToWidget() override;

  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  // IconLabelBubbleView::Delegate:
  SkColor GetIconLabelBubbleSurroundingForegroundColor() const override;
  SkColor GetIconLabelBubbleBackgroundColor() const override;

  // ContentSettingImageViewDelegate:
  bool ShouldHideContentSettingImage() override;
  content::WebContents* GetContentSettingWebContents() override;
  ContentSettingBubbleModelDelegate* GetContentSettingBubbleModelDelegate()
      override;

  // Updates the state of the camera/microphone content-setting icons from the
  // opener WebContents. Called by the host when media-capture state changes.
  void UpdateContentSettingsIcons();

  // Recomputes the outer window bounds now that the Widget (and thus the
  // platform border) exists, so a request that specifies an explicit inner
  // (web-contents) size is honored. Without this, the outer window equals the
  // requested inner size and the top bar eats into the content area, making the
  // window shorter than the Browser-backed PiP window. Must be called by the
  // host *after* Widget::Init() returns, because Init applies the InitParams
  // bounds last and would otherwise clobber the recomputed bounds. Mirrors
  // PictureInPictureBrowserFrameView::OnBrowserViewInitialized.
  void UpdateWindowBoundsForRequestedInnerSize();

  void set_close_reason(CloseReason reason) { close_reason_ = reason; }

 private:
  friend class DocumentPipFrameViewTest;
  class WindowEventObserver;
  // Minimal LocationBarModelDelegate backed by the opener WebContents.
  class LocationBarModelDelegateImpl;
  // Returns the height of the top bar area, including the window top border.
  int GetTopAreaHeight() const;

  // Returns the insets of the window frame borders.
  gfx::Insets FrameBorderInsets() const;

  // Returns the insets of the window frame borders for resizing.
  gfx::Insets ResizeBorderInsets() const;

  // Returns the non-client view area size (border + top bar).
  gfx::Size GetNonClientViewAreaSize() const;

  // Converts `control_view`'s bounds into this frame view's coordinate space
  // for hit testing. `control_view`'s bounds are interpreted in its parent's
  // coordinate space.
  gfx::Rect ConvertControlBoundsToFrame(views::View* control_view) const;

  // Bounds of the origin chip in frame-view coordinates.
  gfx::Rect GetOriginChipBounds() const;

  // Reads the opener URL and security level and updates the window title text,
  // its scheme-dependent elision direction, and the chip's security icon and
  // security chip text.
  void UpdateOriginAndSecurity();

  // Updates the top bar foreground colors based on whether the user is
  // interacting with the window (active) or not (inactive).
  void UpdateTopBarView(bool render_active);

  // Called when mouse entered or exited the PiP window.
  void OnMouseEnteredOrExitedWindow(bool entered);

  // Opens the Page Info dialog for the opener WebContents. Returns false if the
  // dialog could not be shown.
  bool ShowPageInfo();

  // Shows `auto_pip_setting_overlay_` if we have it and have a widget.
  void ShowOverlayIfNeeded();

  // True iff the auto-PiP allow/block overlay exists and is currently visible.
  bool IsOverlayViewVisible() const;

  // Owns this view through its widget/delegate chain and outlives it.
  const raw_ref<DocumentPipHost> host_;

  // Drives the origin chip's URL and security text from the opener WebContents,
  // reusing the omnibox's URL-formatting and secure-display-text logic.
  // `location_bar_model_delegate_` must outlive `location_bar_model_`, which
  // holds a raw pointer to it; declaring the delegate first makes it destroyed
  // last (members are destroyed in reverse declaration order).
  std::unique_ptr<LocationBarModelDelegateImpl> location_bar_model_delegate_;
  std::unique_ptr<LocationBarModelImpl> location_bar_model_;

  raw_ptr<views::FlexLayoutView> top_bar_container_view_ = nullptr;

  // The clickable origin chip (security icon + optional security chip text) to
  // the left of the title area. An IconLabelBubbleView (the omnibox
  // security-chip base), reused for geometry/ink-drop/animation parity but
  // driven from the opener WebContents. Clicking it opens the Page Info dialog.
  raw_ptr<IconLabelBubbleView> origin_chip_ = nullptr;
  // The window title label, showing the opener's URL. A sibling of the origin
  // chip (not a child), so it is excluded from the chip's Page Info click
  // target, mirroring the browser-backed frame's separate window-title label.
  raw_ptr<views::Label> window_title_ = nullptr;

  raw_ptr<views::FlexLayoutView> button_container_view_ = nullptr;

  // The camera/microphone content-setting icons, shown to the left of the
  // window-control buttons. Mirrors PictureInPictureBrowserFrameView's
  // content-setting views but reads from the opener WebContents and uses a
  // null Browser (no feature-promo surface in standalone PiP).
  std::vector<raw_ptr<ContentSettingImageView, VectorExperimental>>
      content_setting_views_;

  raw_ptr<views::ImageButton> back_to_tab_button_ = nullptr;
  raw_ptr<views::ImageButton> close_image_button_ = nullptr;

  // Owned by the view hierarchy via AddChildView(); the raw_ptr is nulled in
  // OnWidgetDestroying(). Mirrors
  // PictureInPictureBrowserFrameView::auto_pip_setting_overlay_.
  raw_ptr<AutoPipSettingOverlayView> auto_pip_setting_overlay_ = nullptr;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  // The security level last applied to the chip, used to decide whether a
  // security-text change should animate (mirrors the omnibox chip).
  security_state::SecurityLevel last_security_level_ = security_state::NONE;

  // Whether the security chip text has been set at least once. The first update
  // never animates (there is no prior state to transition from).
  bool security_text_initialized_ = false;

  // When the window is created and shown for the first time, we render the
  // active window state even if the mouse is not inside it.
  bool render_active_ = true;

  bool mouse_inside_window_ = false;

  // Used to monitor key and mouse events from the native window.
  std::unique_ptr<WindowEventObserver> window_event_observer_;

  CloseReason close_reason_ = CloseReason::kOther;

  // For posting ShowOverlayIfNeeded() from AddedToWidget().
  base::WeakPtrFactory<DocumentPipFrameView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_FRAME_VIEW_H_
