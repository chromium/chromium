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
  raw_ptr<IconLabelBubbleView::Delegate> icon_label_bubble_delegate = nullptr;
  raw_ptr<const gfx::FontList> font_list = nullptr;
  // This is a temporary flag required while transitioning from the legacy
  // page actions framework to the new one.
  // If set to true, `PageActionContainer` will insert spacing on the right to
  // ensure consistent spacing between migrated and legacy page action icons.
  // TODO(crbug.com/384969003): After the page actions migration, this right
  // spacing will no longer be needed.
  bool should_bridge_containers = true;
  // Depending on the surface that use the `PageActionContainer`, the
  // expectation may be different during space constraint. In some cases, the
  // icon should stay to it minimum size. In some others, the icon size should
  // become 0.
  bool hide_icon_on_space_constraint = false;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_VIEW_PARAMS_H_
