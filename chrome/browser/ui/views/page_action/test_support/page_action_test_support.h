// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_PAGE_ACTION_TEST_SUPPORT_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_PAGE_ACTION_TEST_SUPPORT_H_

#include "ui/actions/action_id.h"

class IconLabelBubbleView;

namespace page_actions {
class PageActionViewInterface;

// Helper to get the underlying IconLabelBubbleView from a
// PageActionViewInterface. This uses static_cast and should only be used in
// tests where we know the interface is implemented by a views-based
// PageActionView or PageActionIconView.
IconLabelBubbleView* GetIconLabelBubbleViewForTesting(
    PageActionViewInterface* interface_ptr,
    actions::ActionId action_id);

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_PAGE_ACTION_TEST_SUPPORT_H_
