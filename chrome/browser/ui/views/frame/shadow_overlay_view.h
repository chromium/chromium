// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_SHADOW_OVERLAY_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_SHADOW_OVERLAY_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/view.h"

// This view is responsible for framing the primary elements of the UI when
// toolbar height side panel is showing, providing a nice drop shadow.
class ShadowOverlayView : public views::View, public views::LayoutDelegate {
  METADATA_HEADER(ShadowOverlayView, views::View)

 public:
  explicit ShadowOverlayView(BrowserView& browser_view);
  ~ShadowOverlayView() override;

  class ShadowBox;
  class CornerView;

 private:
  // views::View:
  void VisibilityChanged(View* starting_from, bool visible) override;

  // views::LayoutDelegate:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

  raw_ptr<ShadowBox> shadow_box_ = nullptr;
  raw_ptr<CornerView> top_leading_corner_ = nullptr;
  raw_ptr<CornerView> top_trailing_corner_ = nullptr;
  raw_ptr<CornerView> bottom_leading_corner_ = nullptr;
  raw_ptr<CornerView> bottom_trailing_corner_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_SHADOW_OVERLAY_VIEW_H_
