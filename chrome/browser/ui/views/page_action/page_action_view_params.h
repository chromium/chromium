// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_VIEW_PARAMS_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_VIEW_PARAMS_H_

#include "chrome/browser/ui/views/page_action/page_action_view.h"

namespace page_actions {

// A set of configurable parameters used for creating page action views and
// their container.
struct PageActionViewParams {
  int icon_size = 0;
  gfx::Insets icon_insets;
  int between_icon_spacing = 0;
  const raw_ptr<IconLabelBubbleView::Delegate> icon_label_bubble_delegate =
      nullptr;
};
}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_VIEW_PARAMS_H_
