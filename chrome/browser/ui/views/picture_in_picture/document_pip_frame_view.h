// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_FRAME_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/class_property.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/frame_view.h"

namespace views {
class FlexLayoutView;
class ImageButton;
class Label;
class Widget;
}  // namespace views

// When true, the back-to-tab button is hidden in the PiP frame.
extern const ui::ClassProperty<bool>* const kPipDisallowReturnToOpenerKey;

// DocumentPipFrameView is the non-client frame view for the standalone Document
// Picture-in-Picture widget. It replaces PictureInPictureBrowserFrameView's
// role for the standalone path by inheriting from views::FrameView directly
// (not BrowserFrameView) and taking a views::Widget* instead of a BrowserView*.
//
// Provides:
//   - A title bar with a close button and a back-to-tab button.
//   - NonClientHitTest for proper drag, resize, and button interaction.
class DocumentPipFrameView : public views::FrameView {
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
  // `widget` must outlive this view. Reads kPipDisallowReturnToOpenerKey
  // from the widget to decide whether to show the back-to-tab button.
  explicit DocumentPipFrameView(views::Widget* widget);

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
  void Layout(PassKey) override;

  void set_close_reason(CloseReason reason) { close_reason_ = reason; }

 private:
  friend class DocumentPipFrameViewTest;
  // Returns the height of the top bar area, including the window top border.
  int GetTopAreaHeight() const;

  // Returns the insets of the window frame borders.
  gfx::Insets FrameBorderInsets() const;

  // Returns the insets of the window frame borders for resizing.
  gfx::Insets ResizeBorderInsets() const;

  // Returns the non-client view area size (border + top bar).
  gfx::Size GetNonClientViewAreaSize() const;

  raw_ptr<views::FlexLayoutView> top_bar_container_view_ = nullptr;
  raw_ptr<views::Label> window_title_ = nullptr;
  raw_ptr<views::FlexLayoutView> button_container_view_ = nullptr;
  raw_ptr<views::ImageButton> back_to_tab_button_ = nullptr;
  raw_ptr<views::ImageButton> close_image_button_ = nullptr;

  CloseReason close_reason_ = CloseReason::kOther;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_FRAME_VIEW_H_
