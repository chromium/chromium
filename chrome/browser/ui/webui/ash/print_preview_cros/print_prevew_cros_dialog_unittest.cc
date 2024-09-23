// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/print_preview_cros/print_preview_cros_dialog.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::printing::print_preview {

namespace {

constexpr int64_t kUnguessableTokenHigh = 1;
constexpr int64_t kUnguessableTokenLow = 0;

// Inherits from BrowserWithTestWindowTest to enable calling
// PrintPreviewCrosDialog::ShowDialog to create dialog.
class PrintPreviewCrosDialogTest : public BrowserWithTestWindowTest {
 public:
  PrintPreviewCrosDialogTest() = default;
  ~PrintPreviewCrosDialogTest() override = default;
};

// Verify dialog args contain token passed in when ShowDialog is called.
TEST_F(PrintPreviewCrosDialogTest, DialogArgsContainToken) {
  base::UnguessableToken token = base::UnguessableToken::CreateForTesting(
      kUnguessableTokenHigh, kUnguessableTokenLow);
  raw_ptr<PrintPreviewCrosDialog> dialog =
      PrintPreviewCrosDialog::ShowDialog(token);
  EXPECT_EQ(token.ToString(), dialog->GetDialogArgs());
}

}  // namespace

}  // namespace ash::printing::print_preview
