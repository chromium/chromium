// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SPLIT_VIEW_LAYOUT_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TABS_SPLIT_VIEW_LAYOUT_MENU_MODEL_H_

#include "chrome/browser/ui/tabs/existing_base_sub_menu_model.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/menus/simple_menu_model.h"

class SplitViewLayoutMenuModel : public ui::SimpleMenuModel,
                                 public ui::SimpleMenuModel::Delegate {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kVerticalMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kHorizontalMenuItem);

  // Start command IDs at 1901 to avoid conflicts with other submenus.
  enum class CommandId {
    kVertical = ExistingBaseSubMenuModel::kMinSplitViewLayoutMenuModelCommandId,
    kHorizontal,
  };

  explicit SplitViewLayoutMenuModel(TabStripModel* tab_strip_model,
                                    tabs::TabHandle tab_handle);
  ~SplitViewLayoutMenuModel() override;

  // ui::SimpleMenuModel::Delegate override
  bool IsItemForCommandIdDynamic(int command_id) const override;
  std::u16string GetLabelForCommandId(int command_id) const override;
  ui::ImageModel GetIconForCommandId(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  raw_ptr<TabStripModel> tab_strip_model_ = nullptr;
  tabs::TabHandle tab_handle_;
};

#endif  // CHROME_BROWSER_UI_TABS_SPLIT_VIEW_LAYOUT_MENU_MODEL_H_
