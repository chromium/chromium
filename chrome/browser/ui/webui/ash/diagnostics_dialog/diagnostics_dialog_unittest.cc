// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/diagnostics_dialog/diagnostics_dialog.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "components/session_manager/core/session_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class DiagnosticsDialogTest : public testing::Test {
 public:
  DiagnosticsDialogTest() = default;

  DiagnosticsDialogTest(const DiagnosticsDialogTest&) = delete;
  DiagnosticsDialogTest& operator=(const DiagnosticsDialogTest&) = delete;

  ~DiagnosticsDialogTest() override = default;

  void InitializeDialog(DiagnosticsDialog::DiagnosticsPage page =
                            DiagnosticsDialog::DiagnosticsPage::kDefault) {
    diagnostics_dialog = new DiagnosticsDialog(page);
  }

 protected:
  raw_ptr<DiagnosticsDialog> diagnostics_dialog;

 private:
  session_manager::SessionManager session_manager_;
};

// Test that the Diagnostics dialog can be found once initialized.
TEST_F(DiagnosticsDialogTest, FindInstance) {
  InitializeDialog();
  EXPECT_EQ(diagnostics_dialog,
            SystemWebDialogDelegate::FindInstance(kDiagnosticsDialogId));
}

}  // namespace ash
