// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_dialog_view.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_dialog_view_test_api.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/webauthn/authenticator_request_dialog.h"
#include "chrome/browser/ui/webauthn/authenticator_request_sheet_model.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "content/public/test/browser_test.h"
#include "ui/views/controls/label.h"

namespace {

class TestSheetModel : public AuthenticatorRequestSheetModel {
 public:
  TestSheetModel() = default;

  TestSheetModel(const TestSheetModel&) = delete;
  TestSheetModel& operator=(const TestSheetModel&) = delete;

  ~TestSheetModel() override = default;

  // Getters for data on step specific content:
  std::u16string GetStepSpecificLabelText() { return u"Test Label"; }

 private:
  // AuthenticatorRequestSheetModel:
  bool IsActivityIndicatorVisible() const override { return true; }
  bool IsCancelButtonVisible() const override { return true; }
  std::u16string GetCancelButtonLabel() const override {
    return u"Test Cancel";
  }

  bool IsAcceptButtonVisible() const override { return true; }
  bool IsAcceptButtonEnabled() const override { return true; }
  std::u16string GetAcceptButtonLabel() const override { return u"Test OK"; }

  std::u16string GetStepTitle() const override { return u"Test Title"; }

  std::u16string GetStepDescription() const override {
    return u"Test Description That Is Super Long So That It No Longer Fits On "
           u"One "
           u"Line Because Life Would Be Just Too Simple That Way";
  }

  std::u16string GetError() const override {
    return u"You must construct additional pylons.";
  }

  void OnBack() override {}
  void OnAccept() override {}
  void OnCancel() override {}
  void OnManageDevices() override {}
};

class TestSheetView : public AuthenticatorRequestSheetView {
 public:
  explicit TestSheetView(std::unique_ptr<TestSheetModel> model)
      : AuthenticatorRequestSheetView(std::move(model)) {
    ReInitChildViews();
  }

  TestSheetView(const TestSheetView&) = delete;
  TestSheetView& operator=(const TestSheetView&) = delete;

  ~TestSheetView() override = default;

 private:
  TestSheetModel* test_sheet_model() {
    return static_cast<TestSheetModel*>(model());
  }

  // AuthenticatorRequestSheetView:
  std::pair<std::unique_ptr<views::View>, AutoFocus> BuildStepSpecificContent()
      override {
    return std::make_pair(std::make_unique<views::Label>(
                              test_sheet_model()->GetStepSpecificLabelText()),
                          AutoFocus::kNo);
  }
};

}  // namespace

class StepTransitionObserver
    : public AuthenticatorRequestDialogModel::Observer {
 public:
  StepTransitionObserver() = default;
  int step_transition_count() { return step_transition_count_; }

  // AuthenticatorRequestDialogModel::Observer:
  void OnStepTransition() override { step_transition_count_++; }

 private:
  int step_transition_count_ = 0;
};

class AuthenticatorDialogViewTest : public DialogBrowserTest {
 public:
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    dialog_model_ =
        base::MakeRefCounted<AuthenticatorRequestDialogModel>(nullptr);
    dialog_model_->relying_party_id = "example.com";
    content::WebContents* const web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    // Set the step to a view that is capable of displaying a dialog:
    dialog_model_->SetStep(AuthenticatorRequestDialogModel::Step::kTimedOut);

    StepTransitionObserver step_transition_observer;
    dialog_model_->AddObserver(&step_transition_observer);
    AuthenticatorRequestDialogView* dialog =
        test::AuthenticatorRequestDialogViewTestApi::CreateDialogView(
            web_contents, dialog_model_.get());
    if (name == "default") {
      test::AuthenticatorRequestDialogViewTestApi::ShowWithSheet(
          dialog,
          std::make_unique<TestSheetView>(std::make_unique<TestSheetModel>()));
      EXPECT_EQ(step_transition_observer.step_transition_count(), 0);
    } else if (name == "manage_devices") {
      // Add a paired phone. That should be sufficient for the "Manage
      // devices" button to be shown.
      dialog_model_->mechanisms.emplace_back(
          AuthenticatorRequestDialogModel::Mechanism::Phone("Phone"), u"Phone",
          u"Phone", kSmartphoneIcon, base::DoNothing());
      dialog_model_->SetStep(
          AuthenticatorRequestDialogModel::Step::kMechanismSelection);

      // The "manage devices" button should have been shown on this sheet.
      EXPECT_TRUE(test::AuthenticatorRequestDialogViewTestApi::GetSheet(dialog)
                      ->model()
                      ->IsManageDevicesButtonVisible());
      EXPECT_EQ(step_transition_observer.step_transition_count(), 1);
    }
    dialog_model_->RemoveObserver(&step_transition_observer);
  }

  scoped_refptr<AuthenticatorRequestDialogModel> dialog_model_;
};

// Test the dialog with a custom delegate.
IN_PROC_BROWSER_TEST_F(AuthenticatorDialogViewTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

// Test that the models decide to show the "Manage devices" button when a phone
// is listed.
IN_PROC_BROWSER_TEST_F(AuthenticatorDialogViewTest, InvokeUi_manage_devices) {
  ShowAndVerifyUi();
}
