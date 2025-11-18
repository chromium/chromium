// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_LAYOUT_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_LAYOUT_IMPL_H_

#include <optional>
#include <utility>

#include "chrome/browser/ui/views/frame/layout/browser_view_layout.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_params.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/flex_layout_types.h"

class Browser;

namespace views {
class View;
}

// New browser layout implementation.
//
// This may not work for browsers that are not normal, tabbed browsers;
// `BrowserViewLayoutImplOld` should still be used for other browser types.
class BrowserViewLayoutImpl : public BrowserViewLayout {
 public:
  BrowserViewLayoutImpl(std::unique_ptr<BrowserViewLayoutDelegate> delegate,
                        Browser* browser,
                        BrowserViewLayoutViews views);
  ~BrowserViewLayoutImpl() override;

  // BrowserViewLayout overrides:
  void Layout(views::View* host) override;
  gfx::Size GetMinimumSize(const views::View* host) const override;
  int GetMinWebContentsWidthForTesting() const override;

 private:
  // Hierarchical version of views::ProposedLayout that will allow us to run
  // calculations without actually applying the layout.
  struct ProposedLayout;
  ProposedLayout CalculateProposedLayout(
      const BrowserLayoutParams& params) const;

  // Lay out the top container of the browser. Returns the bounds calculated.
  gfx::Rect CalculateTopContainerLayout(ProposedLayout& layout,
                                        BrowserLayoutParams params,
                                        bool needs_exclusion) const;

  // When the top container is floating, it needs to have its layout applied
  // separately.
  void MaybeLayoutTopContainerOverlay(const BrowserLayoutParams& params);

  // Applies additional clipping and other visual adjustments required to avoid
  // rendering bugs.
  void DoPostLayoutVisualAdjustments();

  // Returns the minimum size of all toolbar-height content except the toolbar-
  // height side panel.
  gfx::Size GetMinimumMainAreaSize() const;

  // Returns the size of the vertical and horizontal tabstrips, as a pair.
  std::pair<gfx::Size, gfx::Size> GetMinimumTabStripSize() const;

  // Returns the type of tabstrip present.
  enum class TabStripType { kNone, kWebUi, kVertical, kHorizontal };
  TabStripType GetTabStripType() const;

  // Returns the type of top separator.
  enum class TopSeparatorType {
    kNone,
    kLoadingBar,
    kTopContainer,
    kMultiContents
  };
  TopSeparatorType GetTopSeparatorType() const;

  // Gets the top of the dialog anchoring area, in local coordinates.
  int GetDialogTop(const ProposedLayout& layout) const;

  // Gets the bottom of the dialog anchoring area, in local coordinates.
  int GetDialogBottom(const ProposedLayout& layout) const;

  // BrowserViewLayout overrides:
  gfx::Point GetDialogPosition(const gfx::Size& dialog_size) const override;
  gfx::Size GetMaximumDialogSize() const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_LAYOUT_IMPL_H_
