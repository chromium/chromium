// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/edit_address_profile_view.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/autofill/edit_address_profile_dialog_controller.h"
#include "chrome/browser/ui/views/autofill/address_editor_view.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/widget/widget.h"

namespace autofill {

// |arg| must be of type base::optional_ref<const AutofillProfile>.
MATCHER_P2(AutofillProfileHasInfo, type, expected_value, "") {
  EXPECT_TRUE(arg.has_value());
  return arg.value().GetRawInfo(type) == expected_value;
}

class MockEditAddressProfileDialogController
    : public EditAddressProfileDialogController {
 public:
  MOCK_METHOD(std::u16string, GetWindowTitle, (), (const, override));
  MOCK_METHOD(const std::u16string&, GetFooterMessage, (), (const, override));
  MOCK_METHOD(std::u16string, GetOkButtonLabel, (), (const, override));
  MOCK_METHOD(const AutofillProfile&, GetProfileToEdit, (), (const, override));
  MOCK_METHOD(bool, GetIsValidatable, (), (const, override));
  MOCK_METHOD(void,
              OnDialogClosed,
              (AutofillClient::AddressPromptUserDecision decision,
               base::optional_ref<const AutofillProfile> profile),
              (override));
};

class EditAddressProfileViewTest : public ChromeViewsTestBase {
 public:
  EditAddressProfileViewTest() = default;
  ~EditAddressProfileViewTest() override = default;

  void CreateViewAndShow() { CreateViewAndShow(address_profile_to_edit()); }

  void CreateViewAndShow(const AutofillProfile& address_profile);

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    address_profile_to_edit_ = test::GetFullProfile();
    test_web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);

    ON_CALL(mock_controller_, GetFooterMessage)
        .WillByDefault(::testing::ReturnRefOfCopy(std::u16string()));
  }

  void TearDown() override {
    dialog_ = nullptr;
    if (widget_) {
      std::exchange(widget_, nullptr)->Close();
    }
    parent_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  const AutofillProfile& address_profile_to_edit() {
    return address_profile_to_edit_;
  }
  content::WebContents* test_web_contents() { return test_web_contents_.get(); }
  EditAddressProfileView* dialog() { return dialog_; }
  MockEditAddressProfileDialogController* mock_controller() {
    return &mock_controller_;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  TestingProfile profile_;
  AutofillProfile address_profile_to_edit_{
      i18n_model_definition::kLegacyHierarchyCountryCode};
  // This enables uses of TestWebContents.
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<content::WebContents> test_web_contents_;
  std::unique_ptr<views::Widget> parent_widget_;
  raw_ptr<views::Widget> widget_ = nullptr;
  raw_ptr<EditAddressProfileView> dialog_ = nullptr;
  testing::NiceMock<MockEditAddressProfileDialogController> mock_controller_;
};

void EditAddressProfileViewTest::CreateViewAndShow(
    const AutofillProfile& address_profile) {
  ON_CALL(*mock_controller(), GetWindowTitle())
      .WillByDefault(testing::Return(std::u16string()));
  ON_CALL(*mock_controller(), GetProfileToEdit())
      .WillByDefault(testing::ReturnRef(address_profile));

  dialog_ = new EditAddressProfileView(mock_controller());
  dialog_->ShowForWebContents(test_web_contents());

  gfx::NativeView parent = gfx::NativeView();
#if BUILDFLAG(IS_MAC)
  // We need a native view parent for the dialog to avoid a DCHECK
  // on Mac.
  parent_widget_ =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  parent = parent_widget_->GetNativeView();
#endif
  widget_ =
      views::DialogDelegate::CreateDialogWidget(dialog_, GetContext(), parent);
  widget_->SetVisibilityChangedAnimationsEnabled(false);
  widget_->Show();
#if BUILDFLAG(IS_MAC)
  // Necessary for Mac. On other platforms this happens in the focus
  // manager, but it's disabled for Mac due to crbug.com/650859.
  parent_widget_->Activate();
  widget_->Activate();
#endif
}

TEST_F(EditAddressProfileViewTest, Sanity) {
  CreateViewAndShow();
  // Check that both OK and cancel button are enabled.
  EXPECT_TRUE(dialog()->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
  EXPECT_TRUE(
      dialog()->IsDialogButtonEnabled(ui::mojom::DialogButton::kCancel));
}

TEST_F(EditAddressProfileViewTest, SaveInvokesTheCallbackWithEditedFullname) {
  CreateViewAndShow();
  const std::u16string kNewFirstName = u"New First Name";
  const std::string locale = g_browser_process->GetApplicationLocale();
  // Confirm that the new name is indeed different from the original one from
  // the controller.
  ASSERT_NE(kNewFirstName, address_profile_to_edit().GetInfo(
                               autofill::FieldType::NAME_FULL, locale));
  AddressEditorView* editor_view = dialog()->GetAddressEditorViewForTesting();
  DCHECK(editor_view);

  editor_view->SetTextInputFieldValueForTesting(autofill::FieldType::NAME_FULL,
                                                kNewFirstName);

  EXPECT_CALL(
      *mock_controller(),
      OnDialogClosed(AutofillClient::AddressPromptUserDecision::kEditAccepted,
                     AutofillProfileHasInfo(autofill::FieldType::NAME_FULL,
                                            kNewFirstName)));
  dialog()->Accept();
}

TEST_F(EditAddressProfileViewTest,
       SaveInvokesTheCallbackWithEditedInvalidPhoneNumber) {
  CreateViewAndShow();
  const std::u16string kNewPhoneNumber = u"123456789";
  const std::string locale = g_browser_process->GetApplicationLocale();
  // Confirm that the new phone number is indeed different from the original one
  // from the controller.
  ASSERT_NE(kNewPhoneNumber,
            address_profile_to_edit().GetInfo(
                autofill::FieldType::PHONE_HOME_WHOLE_NUMBER, locale));

  // Set the phone number in the editor to the new invalid value. Make sure that
  // this value is respected and sent to the backend.
  AddressEditorView* editor_view = dialog()->GetAddressEditorViewForTesting();
  DCHECK(editor_view);

  editor_view->SetTextInputFieldValueForTesting(
      autofill::FieldType::PHONE_HOME_WHOLE_NUMBER, kNewPhoneNumber);

  EXPECT_CALL(
      *mock_controller(),
      OnDialogClosed(
          AutofillClient::AddressPromptUserDecision::kEditAccepted,
          AutofillProfileHasInfo(autofill::FieldType::PHONE_HOME_WHOLE_NUMBER,
                                 kNewPhoneNumber)));
  dialog()->Accept();
}

TEST_F(EditAddressProfileViewTest, SaveInvokesTheCallbackWithEditedEmail) {
  CreateViewAndShow();
  const std::u16string kNewEmail = u"abc@xyz.com";
  const std::string locale = g_browser_process->GetApplicationLocale();
  // Confirm that the new email is indeed different from the original one
  // from the controller.
  ASSERT_NE(kNewEmail, address_profile_to_edit().GetInfo(
                           autofill::FieldType::EMAIL_ADDRESS, locale));
  AddressEditorView* editor_view = dialog()->GetAddressEditorViewForTesting();
  DCHECK(editor_view);

  editor_view->SetTextInputFieldValueForTesting(
      autofill::FieldType::EMAIL_ADDRESS, kNewEmail);

  EXPECT_CALL(
      *mock_controller(),
      OnDialogClosed(AutofillClient::AddressPromptUserDecision::kEditAccepted,
                     AutofillProfileHasInfo(autofill::FieldType::EMAIL_ADDRESS,
                                            kNewEmail)));
  dialog()->Accept();
}

TEST_F(EditAddressProfileViewTest, InvalidFormIsNotSent) {
  ON_CALL(*mock_controller(), GetIsValidatable)
      .WillByDefault(::testing::Return(true));
  CreateViewAndShow(AutofillProfile(AddressCountryCode("US")));

  // `kIgnored` is sent as the decision when the dialog is closed (on test end).
  EXPECT_CALL(
      *mock_controller(),
      OnDialogClosed(AutofillClient::AddressPromptUserDecision::kIgnored,
                     ::testing::_));
  EXPECT_CALL(
      *mock_controller(),
      OnDialogClosed(AutofillClient::AddressPromptUserDecision::kEditAccepted,
                     ::testing::_))
      .Times(0);

  dialog()->Accept();
}

TEST_F(EditAddressProfileViewTest, GetInitiallyFocusedView) {
  auto dialog = std::make_unique<EditAddressProfileView>(mock_controller());

  EXPECT_EQ(dialog->GetInitiallyFocusedView(), nullptr);

  AutofillProfile profile(AddressCountryCode("US"));
  ON_CALL(*mock_controller(), GetProfileToEdit())
      .WillByDefault(testing::ReturnRef(profile));
  dialog->ShowForWebContents(test_web_contents());

  EXPECT_NE(dialog->GetInitiallyFocusedView(), nullptr);
  EXPECT_EQ(
      std::string(
          dialog->GetInitiallyFocusedView()->GetClassMetaData()->type_name()),
      "Combobox");
}

}  // namespace autofill
