// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_VIEW_INTERFACE_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_VIEW_INTERFACE_H_

#include <string>

#include "ui/views/bubble/bubble_anchor.h"
#include "ui/views/controls/button/button.h"

class IconLabelBubbleView;

namespace page_actions {

// A generic interface for page action views to support:
//
// 1. PageActionIconView
// 2. PageActionView
// 3. A WebUI version of PageActionView.
//
// Several UI code sites previously used the view directly, which complicates
// supporting WebUI page actions, where each icon doesn't have its own view.
// Most of these were simply for bubble anchoring (supported now by
// GetBubbleAnchor()), but some also called GetTooltipText(),
// GetAccessibleName(), and SetVisible() instead of  using the
// PageActionController / PageActionModel.
//
// Shifting these to use PageActionController directly can introduce non-trivial
// behavior differences due to subtle differences in nullity of
// PageAction[Icon]Views in the PageAction[Icon]ContainerView), along with
// differences in state management.
//
// On the contrary, while arguably less clean, this approach maintains maximum
// compatibility with the existing callsite behavior.
class PageActionViewInterface {
 public:
  virtual ~PageActionViewInterface() = default;

  // Gets a generic bubble anchor to the page action that works for both views
  // (PageActionIconView and PageActionView) and WebUI-based page action views.
  virtual views::BubbleAnchor GetBubbleAnchor() = 0;

  // Consider using the PageActionController instead of trying to use these view
  // methods, or adding to the methods below.
  virtual std::u16string GetTooltipText() const = 0;
  virtual std::u16string GetAccessibleName() const = 0;
  virtual void SetVisible(bool visible) = 0;

  // For non-WebUI views, returns a pointer to the underlying
  // IconLabelBubbleView. For the WebUI version, returns nullptr.
  //
  // The NotMigrated() method is for production code that only operates on
  // PageActionIconView, never PageActionView -- it will NOTREACHED() in that
  // latter case.
  virtual IconLabelBubbleView* GetIconLabelBubbleViewNotMigrated() = 0;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_VIEW_INTERFACE_H_
