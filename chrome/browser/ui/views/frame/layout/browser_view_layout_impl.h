// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_LAYOUT_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_LAYOUT_IMPL_H_

#include "chrome/browser/ui/views/frame/layout/browser_view_layout.h"
#include "ui/gfx/geometry/size.h"

class Browser;

namespace views {
class View;
}

// New browser layout implementation.
// TODO(http://crbug.com/453717426): Move this to impl file that is only used by
// .cc file.
class BrowserViewLayoutImpl : public BrowserViewLayout {
 public:
  BrowserViewLayoutImpl(std::unique_ptr<BrowserViewLayoutDelegate> delegate,
                        Browser* browser,
                        BrowserViewLayoutViews views);
  ~BrowserViewLayoutImpl() override;

  // BrowserViewLayout overrides:
  void Layout(views::View* host) override;
  gfx::Size GetMinimumSize(const views::View* host) const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_LAYOUT_IMPL_H_
