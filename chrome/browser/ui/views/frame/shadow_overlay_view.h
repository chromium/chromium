// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_SHADOW_OVERLAY_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_SHADOW_OVERLAY_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/side_panel/side_panel_animation_coordinator.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

class BrowserView;
class SidePanel;

// This view is responsible for framing the primary elements of the UI when
// toolbar height side panel is showing, providing a nice drop shadow.
class ShadowOverlayView
    : public views::View,
      public views::LayoutDelegate,
      public SidePanelAnimationCoordinator::AnimationIdObserver,
      public views::ViewObserver {
  METADATA_HEADER(ShadowOverlayView, views::View)

 public:
  explicit ShadowOverlayView(BrowserView& browser_view);
  ~ShadowOverlayView() override;

  class ShadowBox;
  class CornerView;

 private:
  // views::View:
  void VisibilityChanged(View* starting_from, bool visible) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  // views::LayoutDelegate:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

  // SidePanelAnimationCoordinator::AnimationIdObserver
  void OnAnimationSequenceProgressed(
      const SidePanelAnimationCoordinator::SidePanelAnimationId& animation_id,
      double animation_value) override;
  void OnAnimationSequenceEnded(
      const SidePanelAnimationCoordinator::SidePanelAnimationId& animation_id)
      override;

  // views::ViewObserver:
  void OnViewIsDeleting(views::View* observed_view) override;

  raw_ref<BrowserView> browser_view_;
  raw_ptr<ShadowBox> shadow_box_ = nullptr;
  raw_ptr<CornerView> top_leading_corner_ = nullptr;
  raw_ptr<CornerView> top_trailing_corner_ = nullptr;
  raw_ptr<CornerView> bottom_leading_corner_ = nullptr;
  raw_ptr<CornerView> bottom_trailing_corner_ = nullptr;

  base::ScopedObservation<SidePanel, views::ViewObserver> side_panel_observer_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_SHADOW_OVERLAY_VIEW_H_
