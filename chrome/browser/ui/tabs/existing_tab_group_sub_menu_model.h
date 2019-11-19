// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_EXISTING_TAB_GROUP_SUB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TABS_EXISTING_TAB_GROUP_SUB_MENU_MODEL_H_

#include <stddef.h>

#include "base/macros.h"
#include "ui/base/models/simple_menu_model.h"

class TabGroupId;
class TabStripModel;

class ExistingTabGroupSubMenuModel : public ui::SimpleMenuModel,
                                     ui::SimpleMenuModel::Delegate {
 public:
  ExistingTabGroupSubMenuModel(TabStripModel* model, int context_index);
  ~ExistingTabGroupSubMenuModel() override = default;

  bool IsCommandIdChecked(int command_id) const override;

  bool IsCommandIdEnabled(int command_id) const override;

  void ExecuteCommand(int command_id, int event_flags) override;

  // Whether the submenu should be shown in the provided context. True iff
  // the submenu would show at least one group. Does not assume ownership of
  // |model|; |model| must outlive this instance.
  static bool ShouldShowSubmenu(TabStripModel* model, int context_index_);

 private:
  void Build();

  // Unowned; |model_| must outlive this instance.
  TabStripModel* model_;

  int context_index_;

  // Whether the submenu should contain the group |group|. True iff at least
  // one tab that would be affected by the command is not in |group|.
  static bool ShouldShowGroup(TabStripModel* model,
                              int context_index,
                              TabGroupId group);

  DISALLOW_COPY_AND_ASSIGN(ExistingTabGroupSubMenuModel);
};

#endif  // CHROME_BROWSER_UI_TABS_EXISTING_TAB_GROUP_SUB_MENU_MODEL_H_
