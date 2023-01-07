// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/headless/headless_mode_util.h"
#include "chrome/browser/ui/enterprise_startup_dialog.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/switches.h"

namespace policy {

class HeadlessEnterpriseStartupDialogTest : public ::testing::Test {
 public:
  static constexpr char kHeadlessSwitchValue[] = "new";

  HeadlessEnterpriseStartupDialogTest() = default;
  HeadlessEnterpriseStartupDialogTest(
      const HeadlessEnterpriseStartupDialogTest&) = delete;
  HeadlessEnterpriseStartupDialogTest& operator=(
      const HeadlessEnterpriseStartupDialogTest&) = delete;

 protected:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kHeadless, kHeadlessSwitchValue);

    ASSERT_TRUE(headless::IsHeadlessMode());
  }

  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(HeadlessEnterpriseStartupDialogTest, VerifyDialogResetDissmissal) {
  // Expect dialog to be dismissed on destruction.
  base::MockCallback<base::OnceCallback<void(bool, bool)>> callback;
  EXPECT_CALL(callback,
              Run(/*was_accepted=*/false, /*can_show_browser_window=*/true));

  auto dialog = EnterpriseStartupDialog::CreateAndShowDialog(callback.Get());
  dialog->DisplayLaunchingInformationWithThrobber(std::u16string());
  task_environment_.RunUntilIdle();
  dialog.reset();
}

TEST_F(HeadlessEnterpriseStartupDialogTest, VerifyErrorMessageDissmissal) {
  // Expect dialog to be dismissed on error message.
  base::MockCallback<base::OnceCallback<void(bool, bool)>> callback;
  EXPECT_CALL(callback,
              Run(/*was_accepted=*/false, /*can_show_browser_window=*/false));

  auto dialog = EnterpriseStartupDialog::CreateAndShowDialog(callback.Get());
  dialog->DisplayErrorMessage(std::u16string(), std::u16string());
  task_environment_.RunUntilIdle();
  dialog.reset();
}

}  // namespace policy
