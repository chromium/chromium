// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_FRAME_VIEW_CHROMEOS_H_
#define CHROMEOS_UI_FRAME_FRAME_VIEW_CHROMEOS_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ui/frame/header_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/frame_view.h"

namespace display {
class DisplayObserver;
class ScopedDisplayObserver;
enum class TabletState;
}  // namespace display

namespace chromeos {

class FrameViewChromeOS : public views::FrameView,
                          public display::DisplayObserver {
  METADATA_HEADER(FrameViewChromeOS, views::FrameView)

 public:
  explicit FrameViewChromeOS(views::Widget* widget);
  FrameViewChromeOS(const FrameViewChromeOS&) = delete;
  FrameViewChromeOS& operator=(const FrameViewChromeOS&) = delete;
  ~FrameViewChromeOS() override;

  // views::FrameView:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override;
  void ResetWindowControls() override;
  void UpdateWindowTitle() override;
  void SizeConstraintsChanged() override;
  views::View::Views GetChildrenInZOrder() override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void Layout(PassKey) override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void OnThemeChanged() override;

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  // Get the view of the header.
  HeaderView* GetHeaderView();

  // Height from top of window to top of client area.
  int NonClientTopBorderHeight() const;

  bool GetFrameEnabled() const { return frame_enabled_; }

 protected:
  class OverlayView;
  virtual void UpdateDefaultFrameColors();
  // views::FrameView:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;
  const raw_ptr<views::Widget> widget_;

  // View which contains the title and window controls.
  raw_ptr<HeaderView> header_view_ = nullptr;

  raw_ptr<OverlayView> overlay_view_ = nullptr;

  bool frame_enabled_ = true;

 private:
  // Called when |widget_|'s "paint as active" state has changed.
  void PaintAsActiveChanged();

  base::CallbackListSubscription paint_as_active_subscription_ =
      widget_->RegisterPaintAsActiveChangedCallback(
          base::BindRepeating(&FrameViewChromeOS::PaintAsActiveChanged,
                              base::Unretained(this)));

  display::ScopedDisplayObserver display_observer_{this};
};

// View which takes up the entire widget and contains the HeaderView. HeaderView
// is a child of OverlayView to avoid creating a larger texture than necessary
// when painting the HeaderView to its own layer.
class FrameViewChromeOS::OverlayView : public views::View,
                                       public views::ViewTargeterDelegate {
  METADATA_HEADER(OverlayView, views::View)

 public:
  explicit OverlayView(HeaderView* header_view);
  OverlayView(const OverlayView&) = delete;
  OverlayView& operator=(const OverlayView&) = delete;
  ~OverlayView() override;

  // views::View:
  void Layout(PassKey) override;

 private:
  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  raw_ptr<HeaderView> header_view_;
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_FRAME_VIEW_CHROMEOS_H_
