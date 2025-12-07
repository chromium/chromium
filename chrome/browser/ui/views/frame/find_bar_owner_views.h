// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_FIND_BAR_OWNER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_FIND_BAR_OWNER_VIEWS_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/find_bar_owner.h"
#include "ui/gfx/geometry/rect.h"

class BrowserView;

namespace views {
class Widget;
}  // namespace views

// A FindBarOwner implementation that connects FindBarHost to BrowserView.
// This class exposes a subset of BrowserView's functionality to FindBarHost.
class FindBarOwnerViews : public FindBarOwner {
 public:
  explicit FindBarOwnerViews(BrowserView* browser_view);
  ~FindBarOwnerViews() override;

 private:
  // FindBarOwner:
  views::Widget* GetOwnerWidget() override;
  gfx::Rect GetFindBarBoundingBox() override;
  gfx::Rect GetFindBarClippingBox() override;
  bool IsOffTheRecord() const override;
  views::Widget* GetWidgetForAnchoring() override;
  std::u16string GetFindBarAccessibleWindowTitle() override;
  void OnFindBarVisibilityChanged(gfx::Rect visible_bounds) override;
  void CloseOverlappingBubbles() override;

  raw_ptr<BrowserView> browser_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_FIND_BAR_OWNER_VIEWS_H_
