// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_ASSIGN_TO_DESKS_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TOOLBAR_ASSIGN_TO_DESKS_MENU_MODEL_H_

#include "ui/base/models/simple_menu_model.h"

namespace ash {
class DesksHelper;
}

// A menu model that builds the contents of the assign to desks menu.
class AssignToDesksMenuModel : public ui::SimpleMenuModel {
 public:
  explicit AssignToDesksMenuModel(ui::SimpleMenuModel::Delegate* delegate);
  ~AssignToDesksMenuModel() override = default;

  // SimpleMenuModel:
  bool IsVisibleAt(int index) const override;
  base::string16 GetLabelAt(int index) const override;
  bool IsItemCheckedAt(int index) const override;

 private:
  const ash::DesksHelper* const desks_helper_;
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_ASSIGN_TO_DESKS_MENU_MODEL_H_
