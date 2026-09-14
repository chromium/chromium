// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_BLOCK_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_BLOCK_VIEW_H_

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/actions/action_id.h"
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
class AppMenuBlockView : public views::BoxLayoutView {
  METADATA_HEADER(AppMenuBlockView, views::BoxLayoutView)

 public:
  AppMenuBlockView(
      actions::ActionItem* block_action_item,
      views::ActionViewController* action_view_controller,
      base::flat_map<int, raw_ptr<actions::BaseAction>>* command_to_action_map,
      base::RepeatingCallback<void(actions::ActionId)>
          execute_command_callback);
  AppMenuBlockView(const AppMenuBlockView&) = delete;
  AppMenuBlockView& operator=(const AppMenuBlockView&) = delete;
  ~AppMenuBlockView() override;
};

using AppMenuBlockSectionView = AppMenuBlockView;

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_BLOCK_VIEW_H_
