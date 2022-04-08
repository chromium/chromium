// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_menu.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/view.h"
#include "url/gurl.h"

SavedTabGroupMenu::~SavedTabGroupMenu() = default;

SavedTabGroupMenu::SavedTabGroupMenu(const SavedTabGroup* saved_group)
    : ui::SimpleMenuModel(this), saved_group_(saved_group) {}

void SavedTabGroupMenu::BuildMenu() {
  int i = 0;
  auto add_item_with_icon = [&](const SavedTabGroupTab& saved_tab) {
    AddItemWithIcon(i++, saved_tab.tab_title,
                    ui::ImageModel::FromImage(saved_tab.favicon));
  };

  std::for_each(saved_group_->saved_tabs.begin(),
                saved_group_->saved_tabs.end(), add_item_with_icon);
}

void SavedTabGroupMenu::RunMenu(views::Widget* parent,
                                views::MenuButtonController* button_controller,
                                const gfx::Rect& bounds,
                                OpenUrlCallback open_url) {
  DCHECK(parent);
  DCHECK(button_controller);

  open_url_ = std::move(open_url);

  // Instead of building the menu on construction, we can build each saved tab
  // groups context menu when we need it.
  BuildMenu();

  context_menu_runner_ = std::make_unique<views::MenuRunner>(
      this, views::MenuRunner::HAS_MNEMONICS);
  context_menu_runner_->RunMenuAt(parent, button_controller, bounds,
                                  views::MenuAnchorPosition::kTopLeft,
                                  ui::MENU_SOURCE_NONE);
}

bool SavedTabGroupMenu::IsCommandIdEnabled(int command_id) const {
  int num_items = static_cast<int>(saved_group_->saved_tabs.size());
  DCHECK(command_id >= 0 && command_id < num_items);
  return true;
}

void SavedTabGroupMenu::ExecuteCommand(int command_id, int event_flags) {
  DCHECK(IsCommandIdEnabled(command_id));

  const GURL& url_to_open = saved_group_->saved_tabs[command_id].url;
  content::OpenURLParams params(url_to_open, content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                                false /* is_renderer_intiated */,
                                true /* started_from_context_menu */);
  std::move(open_url_).Run(params);
}
