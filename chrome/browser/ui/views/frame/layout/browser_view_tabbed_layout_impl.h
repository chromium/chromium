// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_TABBED_LAYOUT_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_TABBED_LAYOUT_IMPL_H_

#include <memory>
#include <utility>

#include "chrome/browser/ui/animation/browser_animation_types.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_impl.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_params.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/flex_layout_types.h"

class Browser;

namespace views {
class View;
}

// Provides a specialized layout implementation for normal tabbed browsers.
// Should not be used for other types of browsers.
class BrowserViewTabbedLayoutImpl : public BrowserViewLayoutImpl {
 public:
  BrowserViewTabbedLayoutImpl(
      std::unique_ptr<BrowserViewLayoutDelegate> delegate,
      Browser* browser,
      BrowserViewLayoutViews views);
  ~BrowserViewTabbedLayoutImpl() override;

 protected:
  // BrowserViewLayoutImplCommon:
  gfx::Size GetMinimumSize(const views::View* host) const override;
  ProposedLayout CalculateProposedLayout(
      const BrowserLayoutParams& params) const override;
  gfx::Rect CalculateTopContainerLayout(ProposedLayout& layout,
                                        BrowserLayoutParams params,
                                        bool needs_exclusion) const override;
  void OnLayoutParamsChanged(const BrowserLayoutParams& old_params,
                             const BrowserLayoutParams& new_params) override;
  void ConfigureTopContainerBackground(
      const BrowserLayoutParams& params,
      CustomCornersBackground* background) override;
  void DoPreLayoutComputations(const BrowserLayoutParams& params) override;
  void DoPostLayoutVisualAdjustments(
      const BrowserLayoutParams& params) override;
  void DoPostLayoutCleanup() override;

 private:
  struct HorizontalLayout;
  struct SeparatorInfo;
  struct TransientLayoutData;
  struct VerticalTabStripAnimation;

  enum class TabStripType { kNone, kVertical, kHorizontal };

  enum class VerticalTabStripCollapsedState {
    kExpanded,
    kCollapsing,
    kCollapsed,
    kExpanding
  };

  // Gets the amount of padding to place between
  int GetMinimumGrabHandlePadding() const;

  // Returns the minimum size of all toolbar-height content except the side
  // panel.
  gfx::Size GetMinimumMainAreaSize(const BrowserLayoutParams& params) const;

  // Returns the size of the vertical and horizontal tabstrips, as a pair.
  std::pair<gfx::Size, gfx::Size> GetMinimumTabStripSize(
      const BrowserLayoutParams& params) const;

  // Returns the leading margin for the horizontal tab strip region.
  int GetHorizontalTabStripLeadingMargin(
      const BrowserLayoutParams& params) const;

  // Returns whether to make small adjustments to avoid visual "cracking" due to
  // discrepancies between pixel and dip scaling; see
  // https://crbug.com/436278099 for more information on the Pixel Canvas
  // project which aims to permanently avoid this issue.
  bool AvoidCrackingForFractionalDisplay() const;

  // Returns the type of tabstrip present.
  TabStripType GetTabStripType() const;

  // Returns the current state of the vertical tabstrip.
  VerticalTabStripCollapsedState GetVerticalTabStripCollapsedState() const;

  // These helper functions are called during `DoPreLayoutCalculations()` to set
  // up `layout_data_`.
  HorizontalLayout CalculateHorizontalLayout(BrowserLayoutParams& params) const;
  VerticalTabStripAnimation CalculateVerticalTabStripAnimation();
  int GetCollapsedVerticalTabStripRelativeTop() const;
  SeparatorInfo CalculateSeparatorInfo() const;

  std::unique_ptr<TransientLayoutData> layout_data_;

  // Ensure that the tab strip width changes monotonically when expanding.
  mutable int last_vertical_tab_strip_width_ = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_TABBED_LAYOUT_IMPL_H_
