// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_STANDARD_ICON_LABEL_BUBBLE_LAYOUT_STRATEGY_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_STANDARD_ICON_LABEL_BUBBLE_LAYOUT_STRATEGY_H_

#include <optional>

#include "base/time/time.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class SlideAnimation;
}  // namespace gfx

namespace views {
struct ProposedLayout;
class SizeBounds;
}  // namespace views

class StandardIconLabelBubbleAnimationLayoutStrategy
    : public IconLabelBubbleView::IconLabelBubbleAnimationLayoutStrategy {
 public:
  explicit StandardIconLabelBubbleAnimationLayoutStrategy(IconLabelBubbleView& host);
  ~StandardIconLabelBubbleAnimationLayoutStrategy() override;

  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds,
      const IconLabelBubbleView* host) const override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size,
      const IconLabelBubbleView* host) const override;
  void UpdateAnimationProgress(IconLabelBubbleView* host,
                               double progress) override;
  std::optional<base::TimeDelta> GetAnimationDuration(bool show) const override;
  void SetupAnimation(gfx::SlideAnimation* animation, bool show) const override;
  void ResetAnimation(IconLabelBubbleView* host, bool show_label) override;
  void OnAnimationEnded(IconLabelBubbleView* host) override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_STANDARD_ICON_LABEL_BUBBLE_LAYOUT_STRATEGY_H_
