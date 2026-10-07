// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/external_protocol_dialog.h"

#include <memory>
#include <string>

#include "chrome/browser/ui/views/external_protocol_dialog_test_harness.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/window/dialog_client_view.h"

// Tests that keyboard focus works when the dialog is shown. Regression test for
// https://crbug.com/40659150.
IN_PROC_BROWSER_TEST_F(ExternalProtocolDialogBrowserTest, TestFocus) {
  ShowUi(std::string("https://example.test"));

  EXPECT_TRUE(browser()->IsActive());
  EXPECT_TRUE(dialog_->GetCancelButton()->HasFocus());

  // Bypass input protection cooldown.
  dialog_->GetDialogClientView()->ResetViewShownTimeStampForTesting();

  views::test::WidgetDestroyedWaiter waiter(dialog_->GetWidget());
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_SPACE, false,
                                              false, false, false));
  waiter.Wait();
}
