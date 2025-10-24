// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_LAYOUT_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_LAYOUT_IMPL_H_

#include "chrome/browser/ui/views/frame/layout/browser_view_layout.h"
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
  ProposedLayout CalculateProposedLayout() const;

  // BrowserViewLayout overrides:
  gfx::Point GetDialogPosition(const gfx::Size& dialog_size) const override;
  gfx::Size GetMaximumDialogSize() const override;
  int GetDialogTop(const ProposedLayout& layout) const;
  int GetDialogBottom(const ProposedLayout& layout) const;
  views::Span GetDialogHorizontalTarget(const ProposedLayout& layout) const;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_LAYOUT_IMPL_H_
