// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab/tab_context_menu_controller.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

using ::testing::Return;

namespace {
const TabStripModel::ContextMenuCommand kCommand =
    TabStripModel::ContextMenuCommand::CommandReload;
const int kEventFlags = ui::EF_SHIFT_DOWN;
ui::Accelerator kAccelerator(ui::VKEY_CANCEL, ui::EF_SHIFT_DOWN);
}  // namespace

class MockTabContextMenuControllerDelegate
    : public TabContextMenuController::Delegate {
 public:
  MOCK_METHOD(bool,
              IsContextMenuCommandChecked,
              (TabStripModel::ContextMenuCommand command_id),
              (override));
  MOCK_METHOD(bool,
              IsContextMenuCommandEnabled,
              (int index, TabStripModel::ContextMenuCommand command_id),
              (override));
  MOCK_METHOD(bool,
              IsContextMenuCommandAlerted,
              (TabStripModel::ContextMenuCommand command_id),
              (override));
  MOCK_METHOD(void,
              ExecuteContextMenuCommand,
              (int index,
               TabStripModel::ContextMenuCommand command_id,
               int event_flags),
              (override));
  MOCK_METHOD(bool,
              GetContextMenuAccelerator,
              (int command_id, ui::Accelerator* accelerator),
              (override));
};

class TabContextMenuControllerTest : public testing::Test {
 public:
  TabContextMenuControllerTest() = default;
  ~TabContextMenuControllerTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    controller_ =
        std::make_unique<TabContextMenuController>(0, &mock_delegate_);
  }

 protected:
  std::unique_ptr<TabContextMenuController> controller_;
  testing::StrictMock<MockTabContextMenuControllerDelegate> mock_delegate_;
};

// Verifies IsCommandIdChecked's return value depends on the callback.
TEST_F(TabContextMenuControllerTest, VerifyingIsCheckedCallback) {
  EXPECT_CALL(mock_delegate_, IsContextMenuCommandChecked)
      .WillOnce(Return(true));
  EXPECT_TRUE(controller_->IsCommandIdChecked(static_cast<int>(kCommand)));

  EXPECT_CALL(mock_delegate_, IsContextMenuCommandChecked)
      .WillOnce(Return(false));
  EXPECT_FALSE(controller_->IsCommandIdChecked(static_cast<int>(kCommand)));
}

// Verifies IsCommandIdEnabled's return value depends on the callback.
TEST_F(TabContextMenuControllerTest, VerifyingIsEnabledCallback) {
  EXPECT_CALL(mock_delegate_, IsContextMenuCommandEnabled)
      .WillOnce(Return(true));
  EXPECT_TRUE(controller_->IsCommandIdEnabled(static_cast<int>(kCommand)));

  EXPECT_CALL(mock_delegate_, IsContextMenuCommandEnabled)
      .WillOnce(Return(false));
  EXPECT_FALSE(controller_->IsCommandIdEnabled(static_cast<int>(kCommand)));
}

// Verifies that IsCommandIdAlerted's return value depends on the callback.
TEST_F(TabContextMenuControllerTest, VerifyingIsAlertedCallback) {
  EXPECT_CALL(mock_delegate_, IsContextMenuCommandAlerted)
      .WillOnce(Return(true));
  EXPECT_TRUE(controller_->IsCommandIdAlerted(static_cast<int>(kCommand)));

  EXPECT_CALL(mock_delegate_, IsContextMenuCommandAlerted)
      .WillOnce(Return(false));
  EXPECT_FALSE(controller_->IsCommandIdAlerted(static_cast<int>(kCommand)));
}

// Verifies that ExecuteCommand depends on the callback.
TEST_F(TabContextMenuControllerTest, VerifyingExecuteCommandCallback) {
  EXPECT_CALL(mock_delegate_, ExecuteContextMenuCommand);

  // If ExecuteCommand fails to execute the callback the previous EXPECT_CALL
  // will fail.
  controller_->ExecuteCommand(static_cast<int>(kCommand), kEventFlags);
}

// Verifies that GetAcceleratorForCommandId's output accelerator value depends
// on the callback.
TEST_F(TabContextMenuControllerTest, VerifyingGetAcceleratorCallback) {
  EXPECT_CALL(mock_delegate_, GetContextMenuAccelerator)
      .WillOnce([&](int, ui::Accelerator* res_accelerator) {
        *res_accelerator = kAccelerator;
        return true;
      });

  ui::Accelerator result_accelerator;
  EXPECT_TRUE(controller_->GetAcceleratorForCommandId(
      static_cast<int>(kCommand), &result_accelerator));
  EXPECT_EQ(result_accelerator, kAccelerator);
}
