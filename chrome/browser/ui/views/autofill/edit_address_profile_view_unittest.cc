// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/edit_address_profile_view.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/autofill/edit_address_profile_dialog_controller.h"
#include "chrome/browser/ui/views/autofill/address_editor_view.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"

namespace autofill {

// |arg| must be of type AutofillProfile.
MATCHER_P2(AutofillProfileHasInfo, type, expected_value, "") {
  const std::string locale = g_browser_process->GetApplicationLocale();
  return arg.GetInfo(type, locale) == expected_value;
}

class MockEditAddressProfileDialogController
    : public EditAddressProfileDialogController {
 public:
  MOCK_METHOD(std::u16string, GetWindowTitle, (), (const, override));
  MOCK_METHOD(const AutofillProfile&, GetProfileToEdit, (), (const, override));
  MOCK_METHOD(void,
              OnUserDecision,
              (AutofillClient::SaveAddressProfileOfferUserDecision decision,
               const AutofillProfile& profile),
              (override));
  MOCK_METHOD(void, OnDialogClosed, (), (override));
};

class EditAddressProfileViewTest : public ChromeViewsTestBase {
 public:
  EditAddressProfileViewTest();
  ~EditAddressProfileViewTest() override = default;

  void CreateViewAndShow();

  void TearDown() override {
    widget_->Close();
    parent_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  const AutofillProfile& address_profile_to_edit() {
    return address_profile_to_edit_;
  }
  EditAddressProfileView* dialog() { return dialog_; }
  MockEditAddressProfileDialogController* mock_controller() {
    return &mock_controller_;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  TestingProfile profile_;
  AutofillProfile address_profile_to_edit_ = test::GetFullProfile();
  // This enables uses of TestWebContents.
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<content::WebContents> test_web_contents_;
  std::unique_ptr<views::Widget> parent_widget_;
  views::Widget* widget_ = nullptr;
  EditAddressProfileView* dialog_;
  testing::NiceMock<MockEditAddressProfileDialogController> mock_controller_;
};

EditAddressProfileViewTest::EditAddressProfileViewTest() {
  feature_list_.InitAndEnableFeature(
      features::kAutofillAddressProfileSavePrompt);

  test_web_contents_ =
      content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
}

void EditAddressProfileViewTest::CreateViewAndShow() {
  ON_CALL(*mock_controller(), GetWindowTitle())
      .WillByDefault(testing::Return(std::u16string()));
  ON_CALL(*mock_controller(), GetProfileToEdit())
      .WillByDefault(testing::ReturnRef(address_profile_to_edit()));

  dialog_ = new EditAddressProfileView(mock_controller());
  dialog_->ShowForWebContents(test_web_contents_.get());

  gfx::NativeView parent = gfx::kNullNativeView;
#if defined(OS_MAC)
  // We need a native view parent for the dialog to avoid a DCHECK
  // on Mac.
  parent_widget_ = CreateTestWidget();
  parent = parent_widget_->GetNativeView();
#endif
  widget_ =
      views::DialogDelegate::CreateDialogWidget(dialog_, GetContext(), parent);
  widget_->SetVisibilityChangedAnimationsEnabled(false);
  widget_->Show();
#if defined(OS_MAC)
  // Necessary for Mac. On other platforms this happens in the focus
  // manager, but it's disabled for Mac due to crbug.com/650859.
  parent_widget_->Activate();
  widget_->Activate();
#endif
}

TEST_F(EditAddressProfileViewTest, Sanity) {
  CreateViewAndShow();
  // Check that both OK and cancel button are enabled.
  EXPECT_TRUE(dialog()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  EXPECT_TRUE(dialog()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));
}

TEST_F(EditAddressProfileViewTest,
       SaveInvokesTheCallbackWithTheAddressInEditor) {
  CreateViewAndShow();
  const std::u16string kNewFirstName = u"New First Name";
  const std::string locale = g_browser_process->GetApplicationLocale();
  // Confirm that the new name is indeed different from the original one from
  // the controller.
  ASSERT_NE(kNewFirstName, address_profile_to_edit().GetInfo(
                               autofill::ServerFieldType::NAME_FULL, locale));
  AddressEditorView* editor_view = dialog()->GetAddressEditorViewForTesting();
  DCHECK(editor_view);

  editor_view->SetTextInputFieldValueForTesting(
      autofill::ServerFieldType::NAME_FULL, kNewFirstName);

  EXPECT_CALL(
      *mock_controller(),
      OnUserDecision(
          AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted,
          AutofillProfileHasInfo(autofill::ServerFieldType::NAME_FULL,
                                 kNewFirstName)));
  dialog()->Accept();
}

}  // namespace autofill
