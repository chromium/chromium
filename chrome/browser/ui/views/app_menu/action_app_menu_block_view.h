// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_BLOCK_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_BLOCK_VIEW_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace actions {
class ActionItem;
}  // namespace actions

namespace views {
class ActionViewController;
}  // namespace views

// A view containing the block-style section elements (e.g. New Tab, New Window,
// New Incognito Window buttons) for the ActionAppMenu.
class ActionAppMenuBlockView : public views::BoxLayoutView {
  METADATA_HEADER(ActionAppMenuBlockView, views::BoxLayoutView)

 public:
  ActionAppMenuBlockView(
      actions::ActionItem* block_action_item,
      views::ActionViewController* action_view_controller,
      base::flat_map<int, raw_ptr<actions::ActionItem>>* command_to_action_map);
  ActionAppMenuBlockView(const ActionAppMenuBlockView&) = delete;
  ActionAppMenuBlockView& operator=(const ActionAppMenuBlockView&) = delete;
  ~ActionAppMenuBlockView() override;
};

using ActionAppMenuBlockSectionView = ActionAppMenuBlockView;
using AppMenuBlockSectionView = ActionAppMenuBlockView;

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_BLOCK_VIEW_H_
