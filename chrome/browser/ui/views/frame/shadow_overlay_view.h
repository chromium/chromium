// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_SHADOW_OVERLAY_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_SHADOW_OVERLAY_VIEW_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/animation/browser_animation_types.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/view.h"

class BrowserView;
class ShadowFrameView;

// This view is responsible for framing the primary elements of the UI when
// side panel is showing, providing a nice drop shadow.
class ShadowOverlayView : public views::View, public views::LayoutDelegate {
  METADATA_HEADER(ShadowOverlayView, views::View)

 public:
  explicit ShadowOverlayView(BrowserView& browser_view);
  ~ShadowOverlayView() override;

  class CornerView;

 private:
  // views::View:
  void VisibilityChanged(View* starting_from, bool visible) override;
  void AddedToWidget() override;

  // views::LayoutDelegate:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

  void OnAnimationProgressed(const BrowserAnimationController* controller,
                             BrowserAnimationUpdate status);

  double GetShadowValue() const;

  raw_ref<BrowserView> browser_view_;
  raw_ptr<ShadowFrameView> shadow_box_ = nullptr;
  raw_ptr<CornerView> top_leading_corner_ = nullptr;
  raw_ptr<CornerView> top_trailing_corner_ = nullptr;
  raw_ptr<CornerView> bottom_leading_corner_ = nullptr;
  raw_ptr<CornerView> bottom_trailing_corner_ = nullptr;

  base::CallbackListSubscription animation_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_SHADOW_OVERLAY_VIEW_H_
