// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_request_dialog_view.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_dialog_view.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_dialog_view_test_api.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/webauthn/authenticator_request_dialog.h"
#include "chrome/browser/ui/webauthn/authenticator_request_sheet_model.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/label.h"

namespace {

class TestSheetModel : public AuthenticatorRequestSheetModel {
 public:
  TestSheetModel() = default;
  ~TestSheetModel() override = default;

  // Getters for data on step specific content:
  base::string16 GetStepSpecificLabelText() {
    return base::ASCIIToUTF16("Test Label");
  }

 private:
  // AuthenticatorRequestSheetModel:
  bool IsActivityIndicatorVisible() const override { return true; }
  bool IsBackButtonVisible() const override { return true; }
  bool IsCancelButtonVisible() const override { return true; }
  base::string16 GetCancelButtonLabel() const override {
    return base::ASCIIToUTF16("Test Cancel");
  }

  bool IsAcceptButtonVisible() const override { return true; }
  bool IsAcceptButtonEnabled() const override { return true; }
  base::string16 GetAcceptButtonLabel() const override {
    return base::ASCIIToUTF16("Test OK");
  }

  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override {
    return gfx::kNoneIcon;
  }

  base::string16 GetStepTitle() const override {
    return base::ASCIIToUTF16("Test Title");
  }

  base::string16 GetStepDescription() const override {
    return base::ASCIIToUTF16(
        "Test Description That Is Super Long So That It No Longer Fits On One "
        "Line Because Life Would Be Just Too Simple That Way");
  }

  base::Optional<base::string16> GetAdditionalDescription() const override {
    return base::ASCIIToUTF16("More description text.");
  }

  ui::MenuModel* GetOtherTransportsMenuModel() override { return nullptr; }

  void OnBack() override {}
  void OnAccept() override {}
  void OnCancel() override {}

  DISALLOW_COPY_AND_ASSIGN(TestSheetModel);
};

class TestSheetView : public AuthenticatorRequestSheetView {
 public:
  explicit TestSheetView(std::unique_ptr<TestSheetModel> model)
      : AuthenticatorRequestSheetView(std::move(model)) {
    ReInitChildViews();
  }

  ~TestSheetView() override = default;

 private:
  TestSheetModel* test_sheet_model() {
    return static_cast<TestSheetModel*>(model());
  }

  // AuthenticatorRequestSheetView:
  std::unique_ptr<views::View> BuildStepSpecificContent() override {
    return std::make_unique<views::Label>(
        test_sheet_model()->GetStepSpecificLabelText());
  }

  DISALLOW_COPY_AND_ASSIGN(TestSheetView);
};

}  // namespace

class AuthenticatorDialogViewTest : public DialogBrowserTest {
 public:
  AuthenticatorDialogViewTest() = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    content::WebContents* const web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    auto dialog_model = std::make_unique<AuthenticatorRequestDialogModel>(
        /*relying_party_id=*/"example.com");
    dialog_model->SetCurrentStep(
        AuthenticatorRequestDialogModel::Step::kTimedOut);
    AuthenticatorRequestDialogView* dialog =
        test::AuthenticatorRequestDialogViewTestApi::CreateDialogView(
            std::move(dialog_model), web_contents);
    test::AuthenticatorRequestDialogViewTestApi::ShowWithSheet(
        dialog,
        std::make_unique<TestSheetView>(std::make_unique<TestSheetModel>()));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AuthenticatorDialogViewTest);
};

// Test the dialog with a custom delegate.
IN_PROC_BROWSER_TEST_F(AuthenticatorDialogViewTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
