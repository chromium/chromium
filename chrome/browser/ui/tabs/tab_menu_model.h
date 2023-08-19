// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TABS_TAB_MENU_MODEL_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/simple_menu_model.h"

class TabStripModel;
class TabMenuModelDelegate;

// A menu model that builds the contents of the tab context menu. To make sure
// the menu reflects the real state of the tab a new TabMenuModel should be
// created each time the menu is shown.
// IDS in the TabMenuModel cannot overlap. Most menu items will use an ID
// defined in chrome/app/chrome_command_ids.h however dynamic menus will not.
// If adding dynamic IDs to a submenu of this menu, add it to this list
// and make sure the values don't overlap with ranges used by any of the models
// in this list. Also make sure to allocate a fairly large range so you're not
// likely having to expand it later on:
//   ExistingTabGroupSubMenuModel
//   ExistingWindowSubMenuModel
class TabMenuModel : public ui::SimpleMenuModel {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAddANoteTabMenuItem);

  TabMenuModel(ui::SimpleMenuModel::Delegate* delegate,
               TabMenuModelDelegate* tab_menu_model_delegate,
               TabStripModel* tab_strip,
               int index);
  TabMenuModel(const TabMenuModel&) = delete;
  TabMenuModel& operator=(const TabMenuModel&) = delete;
  ~TabMenuModel() override;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAddToNewGroupItemIdentifier);

 private:
  void Build(TabStripModel* tab_strip, int index);
  void BuildForWebApp(TabStripModel* tab_strip, int index);

  std::unique_ptr<ui::SimpleMenuModel> add_to_existing_group_submenu_;
  std::unique_ptr<ui::SimpleMenuModel> add_to_existing_window_submenu_;

  raw_ptr<TabMenuModelDelegate> tab_menu_model_delegate_;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_MENU_MODEL_H_
