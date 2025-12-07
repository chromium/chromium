// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SPLIT_TAB_SWAP_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TABS_SPLIT_TAB_SWAP_MENU_MODEL_H_

#include "chrome/browser/ui/tabs/existing_base_sub_menu_model.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/menus/simple_menu_model.h"

class TabStripModel;

namespace split_tabs {
class SplitTabId;
enum class SplitTabLayout;
}  // namespace split_tabs

class SplitTabSwapMenuModel : public ui::SimpleMenuModel,
                              public ui::SimpleMenuModel::Delegate {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSwapStartTabMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSwapEndTabMenuItem);

  // Start command IDs at 1801 to avoid conflicts with other submenus.
  enum class CommandId {
    kSwapStartTab =
        ExistingBaseSubMenuModel::kMinSplitTabSwapMenuModelCommandId,
    kSwapEndTab,
  };

  explicit SplitTabSwapMenuModel(TabStripModel* tab_strip_model, int tab_index);
  ~SplitTabSwapMenuModel() override;

  // ui::SimpleMenuModel::Delegate override
  bool IsItemForCommandIdDynamic(int command_id) const override;
  std::u16string GetLabelForCommandId(int command_id) const override;
  ui::ImageModel GetIconForCommandId(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  split_tabs::SplitTabId GetSplitTabId() const;
  split_tabs::SplitTabLayout GetSplitLayout() const;

  raw_ptr<TabStripModel> tab_strip_model_ = nullptr;
  int tab_index_;
};

#endif  // CHROME_BROWSER_UI_TABS_SPLIT_TAB_SWAP_MENU_MODEL_H_
