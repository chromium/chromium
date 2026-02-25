// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_TABBED_LAYOUT_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_TABBED_LAYOUT_IMPL_H_

#include <optional>
#include <utility>

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
  void ConfigureTopContainerBackground(
      const BrowserLayoutParams& params,
      CustomCornersBackground* background) override;
  void DoPostLayoutVisualAdjustments(
      const BrowserLayoutParams& params) override;

 private:
  // Gets the amount of padding to place between
  int GetMinimumGrabHandlePadding() const;

  // Returns the minimum size of all toolbar-height content except the toolbar-
  // height side panel.
  gfx::Size GetMinimumMainAreaSize(const BrowserLayoutParams& params) const;

  // Returns the size of the vertical and horizontal tabstrips, as a pair.
  std::pair<gfx::Size, gfx::Size> GetMinimumTabStripSize(
      const BrowserLayoutParams& params) const;

  // Represents the state of the vertical tabstrip.
  enum class VerticalTabStripCollapsedState {
    kExpanded,
    kCollapsing,
    kCollapsed,
    kExpanding
  };

  // Allocate space across the vertical tabstrip, toolbar, and side panels,
  // possibly modifying `params` to allocate grab handle space, and
  // determining how much space to give to each of the left-size elements.
  struct HorizontalLayout {
    VerticalTabStripCollapsedState vertical_tab_strip_collapsed_state =
        VerticalTabStripCollapsedState::kExpanded;
    int vertical_tab_strip_width = 0;
    int toolbar_height_side_panel_width = 0;
    int content_height_side_panel_width = 0;
    int min_content_width = 0;

    // The padding placed around a number of UI elements when the toolbar-height
    // side panel is present.
    int side_panel_padding = 0;

    // In some cases, even when there is a toolbar-height side panel, the top
    // container (containing the toolbar, etc.) are laid out at the top of the
    // screen, above the side panels - this is usually due to other layout
    // constraints.
    bool force_top_container_to_top = false;

    bool has_toolbar_height_side_panel() const {
      return toolbar_height_side_panel_width > 0;
    }
    bool has_content_height_side_panel() const {
      return content_height_side_panel_width > 0;
    }
  };
  HorizontalLayout CalculateHorizontalLayout(BrowserLayoutParams& params) const;

  // Describes how to render the top of the vertical tab strip.
  struct VerticalTabStripAnimation {
    // The y-value of the top of the tab strip.
    int top_offset = 0;
    // The relative size of the top outside corner.
    double top_outside_corner_percent = 0.0;
    // The relative size of the top inside corner.
    double top_inside_corner_percent = 0.0;
  };
  VerticalTabStripAnimation CalculateVerticalTabStripAnimation(
      const BrowserLayoutParams& params) const;

  // Returns the type of tabstrip present.
  enum class TabStripType { kNone, kWebUi, kVertical, kHorizontal };
  TabStripType GetTabStripType() const;

  // Returns whether the shadow overlay (with its attendant margin) around the
  // main area is visible. This is usually tied to the presence of the
  // toolbar-height side panel, but may not be in some browser states.
  bool ShadowOverlayVisible() const;

  // Returns the current state of the vertical tabstrip.
  VerticalTabStripCollapsedState GetVerticalTabStripCollapsedState() const;

  // Returns where the vertical tabstrip starts vertically in collapsed mode.
  // This is relative to the top of the visual client area of `params`, and will
  // be zero if the vertical tabstrip should go all the way to the top of the
  // visual area.
  int GetCollapsedVerticalTabStripRelativeTop(
      const BrowserLayoutParams& params) const;

  // Returns the type of top separator.
  enum class TopSeparatorType {
    kNone,
    kLoadingBar,
    kTopContainer,
    kMultiContents
  };
  TopSeparatorType GetTopSeparatorType() const;

  // Returns the leading margin for the horizontal tab strip region.
  int GetHorizontalTabStripLeadingMargin(
      const BrowserLayoutParams& params) const;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_TABBED_LAYOUT_IMPL_H_
