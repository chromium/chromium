// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crypto_module_password_dialog_view.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/ui/crypto_module_password_dialog.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_base.h"

using CryptoModulePasswordDialogViewTest = ChromeViewsTestBase;

std::unique_ptr<CryptoModulePasswordDialogView> CreateCryptoDialog(
    const CryptoModulePasswordCallback& callback) {
  return std::make_unique<CryptoModulePasswordDialogView>(
      "slot", kCryptoModulePasswordCertEnrollment, "server", callback);
}

TEST_F(CryptoModulePasswordDialogViewTest, AcceptUsesPassword) {
  std::string password;
  auto dialog = CreateCryptoDialog(base::BindLambdaForTesting(
      [&](const std::string& text) { password = text; }));
  EXPECT_EQ(dialog->password_entry_, dialog->GetInitiallyFocusedView());
  EXPECT_TRUE(dialog->GetModalType() != ui::MODAL_TYPE_NONE);

  const std::string kPassword = "diAl0g";
  dialog->password_entry_->SetText(base::ASCIIToUTF16(kPassword));
  EXPECT_TRUE(dialog->Accept());
  EXPECT_EQ(kPassword, password);
}

TEST_F(CryptoModulePasswordDialogViewTest, CancelDoesntUsePassword) {
  std::string password;
  bool callback_run = false;
  auto dialog = CreateCryptoDialog(
      base::BindLambdaForTesting([&](const std::string& text) {
        callback_run = true;
        password = text;
      }));

  const std::string kPassword = "diAl0g";
  dialog->password_entry_->SetText(base::ASCIIToUTF16(kPassword));
  EXPECT_TRUE(dialog->Cancel());
  EXPECT_TRUE(callback_run);
  EXPECT_EQ("", password);
}
