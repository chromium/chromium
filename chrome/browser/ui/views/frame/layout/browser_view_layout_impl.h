// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_LAYOUT_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_LAYOUT_IMPL_H_

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

  // Lay out the main container of the browser.
  void CalculateMainContainerLayout(ProposedLayout& layout,
                                    const BrowserLayoutParams& params,
                                    bool needs_exclusion) const;

  // Lay out the top container of the browser. Returns the bounds calculated.
  gfx::Rect CalculateTopContainerLayout(ProposedLayout& layout,
                                        const BrowserLayoutParams& params,
                                        bool needs_exclusion) const;

  // When the top container is floating, it needs to have its layout applied
  // separately.
  void MaybeLayoutTopContainerOverlay(const BrowserLayoutParams& params);

  // Returns whether the top contents separator should go in the top container.
  bool ContentsSeparatorInTopContainer() const;

  // BrowserViewLayout overrides:
  gfx::Point GetDialogPosition(const gfx::Size& dialog_size) const override;
  gfx::Size GetMaximumDialogSize() const override;
  int GetDialogTop(const ProposedLayout& layout) const;
  int GetDialogBottom(const ProposedLayout& layout) const;
  views::Span GetDialogHorizontalTarget(const ProposedLayout& layout) const;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_LAYOUT_IMPL_H_
