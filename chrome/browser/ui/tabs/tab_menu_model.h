// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TABS_TAB_MENU_MODEL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/menus/simple_menu_model.h"

namespace send_tab_to_self {
class SendTabToSelfContextMenuDelegate;
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
namespace extensions {
class ContextMenuMatcher;
}
#endif

namespace tabs {
class TabInterface;
}

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
//   ExistingComparisonTableSubMenuModel
class TabMenuModel : public ui::SimpleMenuModel {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAddANoteTabMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSplitTabsMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kArrangeSplitTabsMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSwapSplitTabsMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAddNewTabAdjacentMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAddToNewGroupItemIdentifier);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kDuplicateMenuItem);

  TabMenuModel(ui::SimpleMenuModel::Delegate* delegate,
               TabMenuModelDelegate* tab_menu_model_delegate,
               TabStripModel* tab_strip,
               int index);
  TabMenuModel(const TabMenuModel&) = delete;
  TabMenuModel& operator=(const TabMenuModel&) = delete;
  ~TabMenuModel() override;

  // ui::SimpleMenuModel:
  bool IsItemCheckedAt(size_t index) const override;
  bool IsEnabledAt(size_t index) const override;
  bool IsVisibleAt(size_t index) const override;
  void ActivatedAt(size_t index) override;
  void ActivatedAt(size_t index, int event_flags) override;

 private:
  void Build(int index);
  void BuildForWebApp(int index);
  void BuildSendTabToSelfSubmenu(int index);
  void BuildLegacySendTabToSelfItem();
  void AppendGlicItems(int index,
                       int num_tabs,
                       const std::vector<int>& indices);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Support for appending and executing commands for extension items in the
  // tab context menu.
  // Returns the `ContextMenuMatcher` if the item at `index` is an
  // extension-provided item.
  extensions::ContextMenuMatcher* GetMatcherIfExtension(size_t index) const;
#endif

  std::unique_ptr<ui::SimpleMenuModel> add_to_existing_group_submenu_;
  std::unique_ptr<ui::SimpleMenuModel> add_to_existing_window_submenu_;
  std::unique_ptr<ui::SimpleMenuModel>
      add_to_existing_comparison_table_submenu_;
  std::unique_ptr<ui::SimpleMenuModel> swap_with_split_submenu_;
  std::unique_ptr<ui::SimpleMenuModel> split_orientation_submenu_;
  std::unique_ptr<ui::SimpleMenuModel> arrange_split_view_submenu_;
  std::unique_ptr<ui::SimpleMenuModel> glic_tab_sub_menu_model_;
  std::unique_ptr<ui::SimpleMenuModel> send_tab_to_self_submenu_;
  std::unique_ptr<send_tab_to_self::SendTabToSelfContextMenuDelegate>
      send_tab_to_self_submenu_delegate_;

  // `tab_strip_` needs to outlive this class.
  raw_ptr<TabStripModel> tab_strip_;
  raw_ptr<TabMenuModelDelegate> tab_menu_model_delegate_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  std::unique_ptr<extensions::ContextMenuMatcher> extension_items_;
#endif
  // Uses WeakPtr because the menu model outlives the tab in tests. The
  // recommended RegisterWillDetach approach failed to prevent dangling pointers
  // during teardown.
  base::WeakPtr<tabs::TabInterface> tab_interface_;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_MENU_MODEL_H_
