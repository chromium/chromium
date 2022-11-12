// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_NON_CLIENT_FRAME_VIEW_BASE_H_
#define CHROMEOS_UI_FRAME_NON_CLIENT_FRAME_VIEW_BASE_H_

#include "chromeos/ui/frame/header_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

namespace chromeos {

class NonClientFrameViewBase : public views::NonClientFrameView {
 public:
  explicit NonClientFrameViewBase(views::Widget* frame);
  NonClientFrameViewBase(const NonClientFrameViewBase&) = delete;
  NonClientFrameViewBase& operator=(const NonClientFrameViewBase&) = delete;
  ~NonClientFrameViewBase() override;

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
  views::View::Views GetChildrenInZOrder() override;
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void OnThemeChanged() override;

  // Height from top of window to top of client area.
  int NonClientTopBorderHeight() const;

  bool GetFrameEnabled() const { return frame_enabled_; }

 protected:
  class OverlayView;
  virtual void UpdateDefaultFrameColors();
  // views::NonClientFrameView:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;
  views::Widget* const frame_;

  // View which contains the title and window controls.
  HeaderView* header_view_ = nullptr;

  OverlayView* overlay_view_ = nullptr;

  bool frame_enabled_ = true;
};

// View which takes up the entire widget and contains the HeaderView. HeaderView
// is a child of OverlayView to avoid creating a larger texture than necessary
// when painting the HeaderView to its own layer.
class NonClientFrameViewBase::OverlayView : public views::View,
                                            public views::ViewTargeterDelegate {
 public:
  METADATA_HEADER(OverlayView);
  explicit OverlayView(HeaderView* header_view);
  OverlayView(const OverlayView&) = delete;
  OverlayView& operator=(const OverlayView&) = delete;
  ~OverlayView() override;

  // views::View:
  void Layout() override;

 private:
  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  HeaderView* header_view_;
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_NON_CLIENT_FRAME_VIEW_BASE_H_
