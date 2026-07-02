// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_SLIDE_AND_CROSSFADE_ICON_LABEL_BUBBLE_LAYOUT_STRATEGY_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_SLIDE_AND_CROSSFADE_ICON_LABEL_BUBBLE_LAYOUT_STRATEGY_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class SlideAnimation;
}  // namespace gfx

namespace ui {
class Layer;
}  // namespace ui

namespace views {
class ImageView;
struct ProposedLayout;
class SizeBounds;
}  // namespace views

class SlideAndCrossfadeIconLabelBubbleAnimationLayoutStrategy
    : public IconLabelBubbleView::IconLabelBubbleAnimationLayoutStrategy {
 public:
  explicit SlideAndCrossfadeIconLabelBubbleAnimationLayoutStrategy(
      IconLabelBubbleView& host);
  ~SlideAndCrossfadeIconLabelBubbleAnimationLayoutStrategy() override;

  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds,
      const IconLabelBubbleView* host) const override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size,
      const IconLabelBubbleView* host) const override;
  void UpdateAnimationProgress(IconLabelBubbleView* host,
                               double progress) override;
  void OnLayerAdded(IconLabelBubbleView* host, ui::Layer* layer) override;
  void OnLayerRemoved(IconLabelBubbleView* host, ui::Layer* layer) override;
  std::optional<base::TimeDelta> GetAnimationDuration(bool show) const override;
  void SetupAnimation(gfx::SlideAnimation* animation, bool show) const override;
  void ResetAnimation(IconLabelBubbleView* host, bool show_label) override;
  void OnAnimationEnded(IconLabelBubbleView* host) override;
  views::ImageView* GetTrailingImageView() override;

 private:
  struct LayoutDimensions {
    int leading_expanded;
    int spacing_expanded;
    int trailing_expanded;
    int leading_collapsed;
    int spacing_collapsed;
  };

  LayoutDimensions GetLayoutDimensions() const;

  raw_ptr<views::ImageView> trailing_image_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_SLIDE_AND_CROSSFADE_ICON_LABEL_BUBBLE_LAYOUT_STRATEGY_H_
