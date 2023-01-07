// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_MENU_MODEL_TEST_H_
#define CHROME_TEST_BASE_MENU_MODEL_TEST_H_

#include "ui/base/accelerators/accelerator.h"
#include "ui/base/models/simple_menu_model.h"

// A mix-in class to be used in addition to something that derrives from
// testing::Test to provide some extra functionality for testing menu models.
class MenuModelTest {
 public:
  MenuModelTest() {}
  virtual ~MenuModelTest() {}

 protected:
  // A menu delegate that counts the number of times certain things are called
  // to make sure things are hooked up properly.
  class Delegate : public ui::SimpleMenuModel::Delegate {
   public:
    Delegate() : execute_count_(0), enable_count_(0) {}

    bool IsCommandIdChecked(int command_id) const override;
    bool IsCommandIdEnabled(int command_id) const override;
    void ExecuteCommand(int command_id, int event_flags) override;

    int execute_count_;
    mutable int enable_count_;
  };

  // Recursively checks the enabled state and executes a command on every item
  // that's not a separator or a submenu parent item. The returned count should
  // match the number of times the delegate is called to ensure every item
  // works.
  void CountEnabledExecutable(ui::MenuModel* model, int* count);

  Delegate delegate_;
};

#endif  // CHROME_TEST_BASE_MENU_MODEL_TEST_H_
