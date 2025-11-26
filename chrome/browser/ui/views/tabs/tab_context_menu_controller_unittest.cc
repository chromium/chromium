// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_context_menu_controller.h"

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

class TabContextMenuControllerTest : public testing::Test {
 public:
  TabContextMenuControllerTest() = default;
  ~TabContextMenuControllerTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    controller_ = std::make_unique<TabContextMenuController>(
        mock_is_checked_.Get(), mock_is_enabled_.Get(), mock_is_alerted_.Get(),
        mock_execute_.Get(), mock_get_accelerator_.Get());
  }

 protected:
  base::MockCallback<
      base::RepeatingCallback<bool(TabStripModel::ContextMenuCommand)>>
      mock_is_checked_;
  base::MockCallback<
      base::RepeatingCallback<bool(TabStripModel::ContextMenuCommand)>>
      mock_is_enabled_;
  base::MockCallback<
      base::RepeatingCallback<bool(TabStripModel::ContextMenuCommand)>>
      mock_is_alerted_;
  base::MockCallback<
      base::RepeatingCallback<void(TabStripModel::ContextMenuCommand, int)>>
      mock_execute_;
  base::MockCallback<base::RepeatingCallback<bool(int, ui::Accelerator*)>>
      mock_get_accelerator_;

  std::unique_ptr<TabContextMenuController> controller_;
};

// Verifies IsCommandIdChecked's return value depends on the callback.
TEST_F(TabContextMenuControllerTest, VerifyingIsCheckedCallback) {
  EXPECT_CALL(mock_is_checked_, Run(kCommand)).WillOnce(Return(true));
  EXPECT_TRUE(controller_->IsCommandIdChecked(static_cast<int>(kCommand)));

  EXPECT_CALL(mock_is_checked_, Run(kCommand)).WillOnce(Return(false));
  EXPECT_FALSE(controller_->IsCommandIdChecked(static_cast<int>(kCommand)));
}

// Verifies IsCommandIdEnabled's return value depends on the callback.
TEST_F(TabContextMenuControllerTest, VerifyingIsEnabledCallback) {
  EXPECT_CALL(mock_is_enabled_, Run(kCommand)).WillOnce(Return(true));
  EXPECT_TRUE(controller_->IsCommandIdEnabled(static_cast<int>(kCommand)));

  EXPECT_CALL(mock_is_enabled_, Run(kCommand)).WillOnce(Return(false));
  EXPECT_FALSE(controller_->IsCommandIdEnabled(static_cast<int>(kCommand)));
}

// Verifies that IsCommandIdAlerted's return value depends on the callback.
TEST_F(TabContextMenuControllerTest, VerifyingIsAlertedCallback) {
  EXPECT_CALL(mock_is_alerted_, Run(kCommand)).WillOnce(Return(true));
  EXPECT_TRUE(controller_->IsCommandIdAlerted(static_cast<int>(kCommand)));

  EXPECT_CALL(mock_is_alerted_, Run(kCommand)).WillOnce(Return(false));
  EXPECT_FALSE(controller_->IsCommandIdAlerted(static_cast<int>(kCommand)));
}

// Verifies that ExecuteCommand depends on the callback.
TEST_F(TabContextMenuControllerTest, VerifyingExecuteCommandCallback) {
  EXPECT_CALL(mock_execute_, Run(kCommand, kEventFlags));

  // If ExecuteCommand fails to execute the callback the previous EXPECT_CALL
  // will fail.
  controller_->ExecuteCommand(static_cast<int>(kCommand), kEventFlags);
}

// Verifies that GetAcceleratorForCommandId's output accelerator value depends
// on the callback.
TEST_F(TabContextMenuControllerTest, VerifyingGetAcceleratorCallback) {
  EXPECT_CALL(mock_get_accelerator_,
              Run(static_cast<int>(kCommand), ::testing::_))
      .WillOnce([&](int, ui::Accelerator* res_accelerator) {
        *res_accelerator = kAccelerator;
        return true;
      });

  ui::Accelerator result_accelerator;
  EXPECT_TRUE(controller_->GetAcceleratorForCommandId(
      static_cast<int>(kCommand), &result_accelerator));
  EXPECT_EQ(result_accelerator, kAccelerator);
}
