// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_APP_LAYOUT_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_APP_LAYOUT_IMPL_H_

#include <memory>
#include <optional>

#include "chrome/browser/ui/views/frame/layout/browser_view_layout_delegate.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_impl.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"

// Provides a specialized layout implementation for PWA browser windows.
// Should not be used for other types of windows.
class BrowserViewAppLayoutImpl : public BrowserViewLayoutImpl {
 public:
  BrowserViewAppLayoutImpl(std::unique_ptr<BrowserViewLayoutDelegate> delegate,
                           Browser* browser,
                           BrowserViewLayoutViews views);
  ~BrowserViewAppLayoutImpl() override;

 protected:
  // BrowserViewLayoutImplCommon:
  gfx::Size GetMinimumSize(const views::View* host) const override;
  ProposedLayout CalculateProposedLayout(
      const BrowserLayoutParams& params) const override;
  gfx::Rect CalculateTopContainerLayout(ProposedLayout& layout,
                                        BrowserLayoutParams params,
                                        bool needs_exclusion) const override;
  void DoPostLayoutVisualAdjustments() override;

 private:
  // Lays out titlebar elements adding them to `parent`, and updating `params`.
  // It is assumed that this handles the exclusions in `params`.
  void CalculateTitlebarLayout(ProposedLayout& layout,
                               BrowserLayoutParams& params) const;

  mutable std::optional<gfx::Rect> overlay_rect_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_APP_LAYOUT_IMPL_H_
