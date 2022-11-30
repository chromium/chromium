// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/existing_base_sub_menu_model.h"

#include <memory>
#include <vector>

#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/models/simple_menu_model.h"
#include "url/gurl.h"

namespace {

constexpr int kMinCommandId = 1000;
constexpr int kParentNewCommandId = 999;
constexpr int kExpectedFlags = 0xABCD;
constexpr char16_t kItem1Text[] = u"Item1";
constexpr char16_t kTitleText[] = u"Title";
constexpr char16_t kItem2Text[] = u"Item2";

class TestDelegate : public ui::SimpleMenuModel::Delegate {
 public:
  ~TestDelegate() override = default;

  bool new_command_alerted() const { return new_command_alerted_; }
  void set_new_command_alerted(bool alerted) { new_command_alerted_ = alerted; }
  int execute_count() { return execute_count_; }

  bool IsCommandIdAlerted(int command_id) const override {
    EXPECT_EQ(kParentNewCommandId, command_id);
    return new_command_alerted();
  }

  void ExecuteCommand(int command_id, int event_flags) override {
    EXPECT_EQ(kParentNewCommandId, command_id);
    EXPECT_EQ(kExpectedFlags, event_flags);
    ++execute_count_;
  }

 private:
  bool new_command_alerted_ = false;
  int execute_count_ = 0;
};

class TestModel : public ExistingBaseSubMenuModel {
 public:
  TestModel(ui::SimpleMenuModel::Delegate* test_delegate, TabStripModel* model)
      : ExistingBaseSubMenuModel(test_delegate,
                                 model,
                                 0,
                                 kMinCommandId,
                                 kParentNewCommandId) {
    MenuItemInfo info1(kItem1Text);
    info1.target_index = 1;
    infos_.push_back(info1);
    MenuItemInfo title(kTitleText);
    infos_.push_back(title);
    MenuItemInfo info2(kItem2Text);
    info2.target_index = 2;
    infos_.push_back(info2);
    Build(IDS_TAB_CXMENU_SUBMENU_NEW_GROUP, infos_);
  }

  const std::vector<size_t> existing_commands() const {
    return existing_commands_;
  }

 protected:
  void ExecuteExistingCommand(size_t target_index) override {
    existing_commands_.push_back(target_index);
  }

 private:
  std::vector<size_t> existing_commands_;
  std::vector<MenuItemInfo> infos_;
};

}  // namespace

class ExistingBaseSubMenuModelTest : public BrowserWithTestWindowTest {
 public:
  ExistingBaseSubMenuModelTest() = default;
  ~ExistingBaseSubMenuModelTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("chrome://newtab"));
    test_delegate_ = std::make_unique<TestDelegate>();
    test_model_ = std::make_unique<TestModel>(test_delegate_.get(),
                                              browser()->tab_strip_model());
  }

  TestDelegate* test_delegate() const { return test_delegate_.get(); }
  TestModel* test_model() const { return test_model_.get(); }

 private:
  std::unique_ptr<TestDelegate> test_delegate_;
  std::unique_ptr<TestModel> test_model_;
};

// TODO(dfried): Verify that label font list is overridden for title elements.
// Note that we can't test this because UI style info isn't loaded for unit
// tests so it still returns null.

TEST_F(ExistingBaseSubMenuModelTest, IsCommandIdAlerted) {
  EXPECT_FALSE(test_model()->IsCommandIdAlerted(kMinCommandId));
  EXPECT_FALSE(test_model()->IsCommandIdAlerted(kMinCommandId + 1));
  EXPECT_FALSE(test_model()->IsCommandIdAlerted(kMinCommandId + 2));

  test_delegate()->set_new_command_alerted(true);
  EXPECT_TRUE(test_model()->IsCommandIdAlerted(kMinCommandId));
  EXPECT_FALSE(test_model()->IsCommandIdAlerted(kMinCommandId + 1));
  EXPECT_FALSE(test_model()->IsCommandIdAlerted(kMinCommandId + 2));
}

TEST_F(ExistingBaseSubMenuModelTest, ExecuteCommand_New) {
  EXPECT_EQ(0, test_delegate()->execute_count());
  test_model()->ExecuteCommand(kMinCommandId, kExpectedFlags);
  EXPECT_EQ(1, test_delegate()->execute_count());
}

TEST_F(ExistingBaseSubMenuModelTest, ExecuteCommand_Existing) {
  test_model()->ExecuteCommand(kMinCommandId + 1, kExpectedFlags);
  test_model()->ExecuteCommand(kMinCommandId + 2, kExpectedFlags);
  EXPECT_THAT(test_model()->existing_commands(), testing::ElementsAre(1u, 2u));
}
