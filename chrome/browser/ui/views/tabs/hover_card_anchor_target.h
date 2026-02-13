// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_HOVER_CARD_ANCHOR_TARGET_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_HOVER_CARD_ANCHOR_TARGET_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/bubble/bubble_border.h"

class HoverCardAnchorTarget;
struct TabRendererData;

namespace views {
class View;
}  // namespace views

// HoverCardAnchorTarget is a base class for views that display a
// TabHoverCardBubbleView on hover. It supplies the necessary data for
// anchoring and rendering, and can be implemented by views such as Tab
// and VerticalTabView.
class HoverCardAnchorTarget {
 public:
  explicit HoverCardAnchorTarget(views::View* anchor_view);
  virtual ~HoverCardAnchorTarget() = default;

  // Returns true if this target is active.
  virtual bool IsActive() const = 0;

  // Determines if |this| is a valid target.
  virtual bool IsValid() const = 0;

  virtual const TabRendererData& data() const = 0;

  // Helper functions for obtaining a View pointer to |this|.
  virtual views::View* GetAnchorView();
  virtual const views::View* GetAnchorView() const;
  static HoverCardAnchorTarget* FromAnchorView(views::View* anchor_view);

  virtual views::BubbleBorder::Arrow GetAnchorPosition() const = 0;

 private:
  raw_ptr<views::View> anchor_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_HOVER_CARD_ANCHOR_TARGET_H_
