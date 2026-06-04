// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_FRAME_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/frame_view.h"

namespace views {
class Button;
class FlexLayoutView;
class ImageButton;
class ImageView;
class Label;
class Widget;
}  // namespace views

class DocumentPipHost;

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
// Unlike PictureInPictureBrowserFrameView, this view does NOT pull in the
// omnibox stack (LocationBarModel / LocationIconView). The origin text and
// security icon are derived directly from the opener WebContents via a small
// PiP-specific origin chip, so the view avoids a dependency on the
// //chrome/browser/ui monolith and the associated circular include. Page Info
// is opened through the //chrome/browser/ui/views/page_info bubble stack.
class DocumentPipFrameView : public views::FrameView,
                             public views::WidgetObserver {
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

  void set_close_reason(CloseReason reason) { close_reason_ = reason; }

 private:
  friend class DocumentPipFrameViewTest;
  class WindowEventObserver;
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

  // Reads the opener URL and security level and updates the origin label text
  // and the lock/security icon.
  void UpdateOriginAndSecurity();

  // Updates the top bar foreground colors based on whether the user is
  // interacting with the window (active) or not (inactive).
  void UpdateTopBarView(bool render_active);

  // Called when mouse entered or exited the PiP window.
  void OnMouseEnteredOrExitedWindow(bool entered);

  // Opens the Page Info dialog for the opener WebContents. Returns false if the
  // dialog could not be shown.
  bool ShowPageInfo();

  // Owns this view through its widget/delegate chain and outlives it.
  const raw_ref<DocumentPipHost> host_;

  raw_ptr<views::FlexLayoutView> top_bar_container_view_ = nullptr;

  // The clickable origin chip (lock icon + origin label) to the left of the
  // title area. Clicking it opens the Page Info dialog.
  raw_ptr<views::Button> origin_chip_ = nullptr;
  raw_ptr<views::ImageView> security_icon_ = nullptr;
  raw_ptr<views::Label> origin_label_ = nullptr;

  raw_ptr<views::FlexLayoutView> button_container_view_ = nullptr;

  raw_ptr<views::ImageButton> back_to_tab_button_ = nullptr;
  raw_ptr<views::ImageButton> close_image_button_ = nullptr;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  // When the window is created and shown for the first time, we render the
  // active window state even if the mouse is not inside it.
  bool render_active_ = true;

  bool mouse_inside_window_ = false;

  // Used to monitor key and mouse events from the native window.
  std::unique_ptr<WindowEventObserver> window_event_observer_;

  CloseReason close_reason_ = CloseReason::kOther;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_FRAME_VIEW_H_
