// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crypto_module_password_dialog_view.h"

#include <string>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/crypto_module_password_dialog.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_base.h"

class CryptoModulePasswordDialogViewTest : public ChromeViewsTestBase {
 public:
  CryptoModulePasswordDialogViewTest() {}
  ~CryptoModulePasswordDialogViewTest() override {}

  void Capture(const std::string& text) {
    text_ = text;
  }

  void CreateCryptoDialog(const CryptoModulePasswordCallback& callback) {
    dialog_ = std::make_unique<CryptoModulePasswordDialogView>(
        "slot", kCryptoModulePasswordCertEnrollment, "server", callback);
  }

  std::string text_;
  std::unique_ptr<CryptoModulePasswordDialogView> dialog_;
};

TEST_F(CryptoModulePasswordDialogViewTest, TestAccept) {
  CryptoModulePasswordCallback cb(
      base::Bind(&CryptoModulePasswordDialogViewTest::Capture,
                 base::Unretained(this)));
  CreateCryptoDialog(cb);
  EXPECT_EQ(dialog_->password_entry_, dialog_->GetInitiallyFocusedView());
  EXPECT_TRUE(dialog_->GetModalType() != ui::MODAL_TYPE_NONE);
  const std::string kPassword = "diAl0g";
  dialog_->password_entry_->SetText(base::ASCIIToUTF16(kPassword));
  EXPECT_TRUE(dialog_->Accept());
  EXPECT_EQ(kPassword, text_);
  const base::string16 empty;
  EXPECT_EQ(empty, dialog_->password_entry_->GetText());
}
