// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crypto_module_password_dialog_view.h"

#include <string>

#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/crypto_module_password_dialog.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_client_view.h"

using CryptoModulePasswordDialogViewTest = ChromeViewsTestBase;

std::unique_ptr<CryptoModulePasswordDialogView> CreateCryptoDialog(
    CryptoModulePasswordCallback callback) {
  return std::make_unique<CryptoModulePasswordDialogView>(
      "slot", kCryptoModulePasswordCertEnrollment, "server",
      std::move(callback));
}

TEST_F(CryptoModulePasswordDialogViewTest, AcceptUsesPassword) {
  std::string password;
  auto dialog = CreateCryptoDialog(base::BindLambdaForTesting(
      [&](const std::string& text) { password = text; }));
  EXPECT_EQ(dialog->password_entry_, dialog->GetInitiallyFocusedView());
  EXPECT_TRUE(dialog->GetModalType() != ui::mojom::ModalType::kNone);

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

class WidgetCloseWaiter : public views::WidgetObserver {
 public:
  explicit WidgetCloseWaiter(views::Widget* widget) {
    observation_.Observe(widget);
  }
  ~WidgetCloseWaiter() override {}

  void Wait() { loop_.Run(); }

  void OnWidgetDestroying(views::Widget* widget) override {
    CHECK_EQ(widget, observation_.GetSource());
    observation_.Reset();
    loop_.Quit();
  }

 private:
  base::ScopedObservation<views::Widget, views::WidgetObserver> observation_{
      this};
  base::RunLoop loop_;
};

TEST_F(CryptoModulePasswordDialogViewTest, EscapeRunsCallback) {
  bool callback_run = false;
  auto dialog = CreateCryptoDialog(base::BindLambdaForTesting(
      [&](const std::string&) { callback_run = true; }));
  auto* weak_dialog = dialog.get();

  views::Widget* dialog_widget =
      dialog->CreateDialogWidget(std::move(dialog), GetContext(), nullptr);

  views::DialogClientView* dcv = weak_dialog->GetDialogClientView();
  ASSERT_TRUE(dcv);

  WidgetCloseWaiter waiter(dialog_widget);

  dcv->AcceleratorPressed(ui::Accelerator(ui::VKEY_ESCAPE, 0));

  waiter.Wait();
  EXPECT_TRUE(callback_run);
}
