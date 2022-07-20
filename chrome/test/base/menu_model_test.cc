// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/menu_model_test.h"
#include "testing/gtest/include/gtest/gtest.h"


bool MenuModelTest::Delegate::IsCommandIdChecked(int command_id) const {
  return false;
}

bool MenuModelTest::Delegate::IsCommandIdEnabled(int command_id) const {
  ++enable_count_;
  return true;
}

void MenuModelTest::Delegate::ExecuteCommand(int command_id, int event_flags) {
  ++execute_count_;
}

// Recursively checks the enabled state and executes a command on every item
// that's not a separator or a submenu parent item. The returned count should
// match the number of times the delegate is called to ensure every item works.
void MenuModelTest::CountEnabledExecutable(ui::MenuModel* model,
                                           int* count) {
  for (int i = 0; i < model->GetItemCount(); ++i) {
    ui::MenuModel::ItemType type = model->GetTypeAt(i);
    switch (type) {
      case ui::MenuModel::TYPE_SEPARATOR:
        continue;
      case ui::MenuModel::TYPE_SUBMENU:
        CountEnabledExecutable(model->GetSubmenuModelAt(i), count);
        break;
      case ui::MenuModel::TYPE_COMMAND:
      case ui::MenuModel::TYPE_CHECK:
      case ui::MenuModel::TYPE_RADIO:
        model->IsEnabledAt(i);  // Check if it's enabled (ignore answer).
        model->ActivatedAt(i);  // Execute it.
        (*count)++;  // Increment the count of executable items seen.
        break;
      default:
        FAIL();  // Ensure every case is tested.
    }
  }
}
