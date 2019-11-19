// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TABS_TAB_MENU_MODEL_H_

#include "base/macros.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_sub_menu_model.h"
#include "ui/base/models/simple_menu_model.h"

class TabStripModel;

// A menu model that builds the contents of the tab context menu. To make sure
// the menu reflects the real state of the tab a new TabMenuModel should be
// created each time the menu is shown.
class TabMenuModel : public ui::SimpleMenuModel {
 public:
  // Range of command IDs to use for the items in the send tab to self submenu.
  static const int kMinSendTabToSelfSubMenuCommandId =
      send_tab_to_self::SendTabToSelfSubMenuModel::kMinCommandId;
  static const int kMaxSendTabToSelfSubMenuCommandId =
      send_tab_to_self::SendTabToSelfSubMenuModel::kMaxCommandId;

  TabMenuModel(ui::SimpleMenuModel::Delegate* delegate,
               TabStripModel* tab_strip,
               int index);
  ~TabMenuModel() override;

 private:
  void Build(TabStripModel* tab_strip, int index);

  std::unique_ptr<ui::SimpleMenuModel> add_to_existing_group_submenu_;

  // Send tab to self submenu.
  std::unique_ptr<send_tab_to_self::SendTabToSelfSubMenuModel>
      send_tab_to_self_sub_menu_model_;

  DISALLOW_COPY_AND_ASSIGN(TabMenuModel);
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_MENU_MODEL_H_
