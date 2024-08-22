// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/input_method/input_method_menu_manager.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {
namespace ime {

TEST(InputMethodMenuManagerTest, TestGetSingleton) {
  EXPECT_TRUE(InputMethodMenuManager::GetInstance());
}

class MockObserver : public InputMethodMenuManager::Observer {
 public:
  MockObserver() : input_method_menu_item_changed_count_(0) {}
  ~MockObserver() override {}

  // Called when the list of menu items is changed.
  void InputMethodMenuItemChanged(InputMethodMenuManager* manager) override {
    input_method_menu_item_changed_count_++;
  }
  int input_method_menu_item_changed_count_;
};

class InputMethodMenuManagerStatefulTest : public testing::Test {
 public:
  InputMethodMenuManagerStatefulTest() : observer_(new MockObserver()) {}
  ~InputMethodMenuManagerStatefulTest() override {}
  void SetUp() override {
    menu_manager_ = InputMethodMenuManager::GetInstance();
    menu_manager_->AddObserver(observer_.get());
  }

  void TearDown() override { menu_manager_->RemoveObserver(observer_.get()); }

  raw_ptr<InputMethodMenuManager> menu_manager_;
  std::unique_ptr<MockObserver> observer_;
};

TEST_F(InputMethodMenuManagerStatefulTest, AddAndObserve) {
  EXPECT_EQ(observer_->input_method_menu_item_changed_count_, 0);
  menu_manager_->SetCurrentInputMethodMenuItemList(InputMethodMenuItemList());
  EXPECT_EQ(observer_->input_method_menu_item_changed_count_, 1);
}

TEST_F(InputMethodMenuManagerStatefulTest, AddAndCheckExists) {
  InputMethodMenuItemList list;
  list.push_back(InputMethodMenuItem("key1", "label1", false));
  list.push_back(InputMethodMenuItem("key2", "label2", false));
  menu_manager_->SetCurrentInputMethodMenuItemList(list);
  EXPECT_EQ(menu_manager_->GetCurrentInputMethodMenuItemList().size(), 2U);
  EXPECT_EQ(menu_manager_->GetCurrentInputMethodMenuItemList().at(0).ToString(),
            "key=key1, label=label1, "
            "is_selection_item_checked=0");
  EXPECT_EQ(menu_manager_->GetCurrentInputMethodMenuItemList().at(1).ToString(),
            "key=key2, label=label2, "
            "is_selection_item_checked=0");

  EXPECT_TRUE(menu_manager_->HasInputMethodMenuItemForKey("key1"));
  EXPECT_TRUE(menu_manager_->HasInputMethodMenuItemForKey("key2"));
  EXPECT_FALSE(menu_manager_->HasInputMethodMenuItemForKey("key-not-exist"));
}

}  // namespace ime
}  // namespace ui
