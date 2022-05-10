// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_dialog.h"

#include "ash/shell.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_browsertest_base.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace chromeos {

using ParentAccessDialogBrowserTest = ParentAccessChildUserBrowserTestBase;

// Verify that the dialog is shown and correctly configured.
IN_PROC_BROWSER_TEST_F(ParentAccessDialogBrowserTest, ShowDialog) {
  // Show the dialog.
  ParentAccessDialog::ShowError error = chromeos::ParentAccessDialog::Show();

  // Verify it is showing.
  ASSERT_EQ(error, ParentAccessDialog::ShowError::kNone);
  ASSERT_NE(chromeos::ParentAccessDialog::GetInstance(), nullptr);

  // Verify that it is correctly configured.
  EXPECT_EQ(
      chromeos::ParentAccessDialog::GetInstance()->GetDialogContentURL().spec(),
      chrome::kChromeUIParentAccessURL);
  EXPECT_TRUE(
      chromeos::ParentAccessDialog::GetInstance()->ShouldShowCloseButton());
  EXPECT_EQ(chromeos::ParentAccessDialog::GetInstance()->GetDialogModalType(),
            ui::ModalType::MODAL_TYPE_SYSTEM);

  // Send ESCAPE keypress.  EventGenerator requires the root window, which has
  // to be fetched from the Ash shell.
  ui::test::EventGenerator generator(ash::Shell::Get()->GetPrimaryRootWindow());
  generator.PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);

  // The dialog instance should be gone after ESC is pressed.
  EXPECT_EQ(chromeos::ParentAccessDialog::GetInstance(), nullptr);
}

IN_PROC_BROWSER_TEST_F(ParentAccessDialogBrowserTest,
                       ErrorOnDialogAlreadyVisible) {
  // Show the dialog.
  ParentAccessDialog::ShowError error = chromeos::ParentAccessDialog::Show();

  // Verify it is showing.
  ASSERT_EQ(error, ParentAccessDialog::ShowError::kNone);
  ASSERT_NE(chromeos::ParentAccessDialog::GetInstance(), nullptr);

  error = chromeos::ParentAccessDialog::Show();

  // Verify an error was returned indicating it can't be shown again.
  EXPECT_EQ(error, ParentAccessDialog::ShowError::kDialogAlreadyVisible);
  EXPECT_NE(chromeos::ParentAccessDialog::GetInstance(), nullptr);
}

using ParentAccessDialogRegularUserBrowserTest =
    ParentAccessRegularUserBrowserTestBase;

// Verify that the dialog is not shown for non child users.
IN_PROC_BROWSER_TEST_F(ParentAccessDialogRegularUserBrowserTest,
                       ErrorForNonChildUser) {
  // Show the dialog.
  ParentAccessDialog::ShowError error = chromeos::ParentAccessDialog::Show();

  // Verify it is not showing.
  EXPECT_EQ(error, ParentAccessDialog::ShowError::kNotAChildUser);
  EXPECT_EQ(chromeos::ParentAccessDialog::GetInstance(), nullptr);
}

}  // namespace chromeos
