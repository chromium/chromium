// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_request_dialog_view.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_dialog_view_test_api.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/webauthn/authenticator_request_dialog.h"
#include "chrome/browser/ui/webauthn/authenticator_request_sheet_model.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "device/fido/features.h"
#include "ui/gfx/paint_vector_icon.h"
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
  bool IsBackButtonVisible() const override { return true; }
  bool IsCancelButtonVisible() const override { return true; }
  std::u16string GetCancelButtonLabel() const override {
    return u"Test Cancel";
  }

  bool IsAcceptButtonVisible() const override { return true; }
  bool IsAcceptButtonEnabled() const override { return true; }
  std::u16string GetAcceptButtonLabel() const override { return u"Test OK"; }

  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override {
    return gfx::kNoneIcon;
  }

  std::u16string GetStepTitle() const override { return u"Test Title"; }

  std::u16string GetStepDescription() const override {
    return u"Test Description That Is Super Long So That It No Longer Fits On "
           u"One "
           u"Line Because Life Would Be Just Too Simple That Way";
  }

  std::u16string GetAdditionalDescription() const override {
    return u"More description text.";
  }

  std::u16string GetError() const override {
    return u"You must construct additional pylons.";
  }

  ui::MenuModel* GetOtherMechanismsMenuModel() override { return nullptr; }

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

class AuthenticatorDialogViewTest : public DialogBrowserTest {
 public:
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    dialog_model_ = std::make_unique<AuthenticatorRequestDialogModel>(
        /*web_contents=*/nullptr);
    dialog_model_->set_relying_party_id("example.com");

    if (name == "default") {
      dialog_model_->StartFlow(
          device::FidoRequestHandlerBase::TransportAvailabilityInfo(),
          /*use_location_bar_bubble=*/false,
          /*prefer_native_api=*/false);
      dialog_model_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kTimedOut);
      content::WebContents* const web_contents =
          browser()->tab_strip_model()->GetActiveWebContents();
      AuthenticatorRequestDialogView* dialog =
          test::AuthenticatorRequestDialogViewTestApi::CreateDialogView(
              web_contents, dialog_model_.get());
      test::AuthenticatorRequestDialogViewTestApi::ShowWithSheet(
          dialog,
          std::make_unique<TestSheetView>(std::make_unique<TestSheetModel>()));
    } else if (name == "manage_devices") {
      // Enable caBLE and add a paired phone. That should be sufficient for the
      // "Manage devices" button to be shown.
      device::FidoRequestHandlerBase::TransportAvailabilityInfo
          transport_availability;
      transport_availability.available_transports = {
          AuthenticatorTransport::kUsbHumanInterfaceDevice,
          AuthenticatorTransport::kHybrid};

      std::array<uint8_t, device::kP256X962Length> public_key = {0};
      AuthenticatorRequestDialogModel::PairedPhone phone("Phone", 0,
                                                         public_key);
      dialog_model_->set_cable_transport_info(
          /*extension_is_v2=*/absl::nullopt,
          /*paired_phones=*/{phone},
          /*contact_phone_callback=*/base::DoNothing(), "fido://qrcode");
      dialog_model_->StartFlow(std::move(transport_availability),
                               /*use_location_bar_bubble=*/false,
                               /*prefer_native_api=*/false);

      // The dialog is owned by the Views hierarchy so this is a non-owning
      // pointer.
      AuthenticatorRequestDialogView* dialog =
          test::AuthenticatorRequestDialogViewTestApi::CreateDialogView(
              browser()->tab_strip_model()->GetActiveWebContents(),
              dialog_model_.get());

      // The "manage devices" button should have been shown on this sheet.
      EXPECT_EQ(
          reinterpret_cast<AuthenticatorSheetModelBase*>(
              test::AuthenticatorRequestDialogViewTestApi::GetSheet(dialog)
                  ->model())
              ->dialog_model()
              ->current_step(),
          AuthenticatorRequestDialogModel::Step::kMechanismSelection);
      EXPECT_TRUE(test::AuthenticatorRequestDialogViewTestApi::GetSheet(dialog)
                      ->model()
                      ->IsManageDevicesButtonVisible());
    }
  }

  std::unique_ptr<AuthenticatorRequestDialogModel> dialog_model_;
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
