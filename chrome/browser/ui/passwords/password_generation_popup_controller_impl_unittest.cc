// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_generation_popup_controller_impl.h"

#include <memory>
#include <string>

#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/passwords/password_generation_popup_controller.h"
#include "chrome/browser/ui/passwords/password_generation_popup_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/ui/suggestion_hiding_reason.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_f.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/test/scoped_feature_list.h"
#include "components/password_manager/core/browser/features/password_features.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace password_manager {
namespace {

using autofill::password_generation::PasswordGenerationType;
using autofill::password_generation::PasswordGenerationUIData;
using ::testing::_;
using ::testing::Return;

PasswordGenerationUIData CreatePasswordGenerationUIData() {
  return PasswordGenerationUIData(
      gfx::RectF(100, 20), /*max_length=*/20, u"element",
      autofill::FieldRendererId(100),
      /*is_generation_element_password_type=*/true, base::i18n::TextDirection(),
      autofill::FormData(), /*input_field_empty=*/true);
}

class MockPasswordManagerDriver
    : public password_manager::StubPasswordManagerDriver {
 public:
  MOCK_METHOD(void,
              GeneratedPasswordAccepted,
              (const std::u16string&),
              (override));
  MOCK_METHOD(void,
              GeneratedPasswordAccepted,
              (const autofill::FormData&,
               autofill::FieldRendererId,
               const std::u16string&),
              (override));
  MOCK_METHOD(PasswordGenerationFrameHelper*,
              GetPasswordGenerationHelper,
              (),
              (override));
  MOCK_METHOD(void,
              PreviewGenerationSuggestion,
              (const std::u16string& password),
              (override));
  MOCK_METHOD(void, ClearPreviewedForm, (), (override));
  MOCK_METHOD(void, FocusNextFieldAfterPasswords, (), (override));
};

class MockPasswordGenerationPopupView : public PasswordGenerationPopupView {
 public:
  MOCK_METHOD(bool, Show, (), (override));
  MOCK_METHOD(void, Hide, (), (override));
  MOCK_METHOD(void, UpdateState, (), (override));
  MOCK_METHOD(void, UpdateGeneratedPasswordValue, (), (override));
  MOCK_METHOD(bool, UpdateBoundsAndRedrawPopup, (), (override));
  MOCK_METHOD(void, PasswordSelectionUpdated, (), (override));
  MOCK_METHOD(void, NudgePasswordSelectionUpdated, (), (override));
};

class PasswordGenerationPopupControllerImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override;
  void TearDown() override;

  std::unique_ptr<MockPasswordManagerDriver> CreateDriver();

 protected:
  MockPasswordGenerationPopupView* popup_view() { return &view_; }
  MockPasswordManagerDriver& driver() { return *driver_; }
  base::WeakPtr<PasswordManagerDriver> weak_driver() {
    return driver_->AsWeakPtr();
  }
  content::WebContents* web_contents() { return web_contents_.get(); }
  PasswordGenerationUIData& ui_data() { return ui_data_; }

 private:
  StubPasswordManagerClient client_;
  std::unique_ptr<MockPasswordManagerDriver> driver_;
  std::unique_ptr<PasswordGenerationFrameHelper> pw_generation_helper_;
  std::unique_ptr<content::WebContents> web_contents_;
  PasswordGenerationUIData ui_data_;
  MockPasswordGenerationPopupView view_;
};

void PasswordGenerationPopupControllerImplTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  driver_ = CreateDriver();
  web_contents_ = CreateTestWebContents();
  ui_data_ = CreatePasswordGenerationUIData();

  // The password generation helper is needed in the offer generation state and
  // since the driver mock returns a raw pointer to it, we construct it first.
  pw_generation_helper_ =
      std::make_unique<PasswordGenerationFrameHelper>(&client_, driver_.get());
  ON_CALL(driver(), GetPasswordGenerationHelper)
      .WillByDefault(Return(pw_generation_helper_.get()));
}

void PasswordGenerationPopupControllerImplTest::TearDown() {
  web_contents_.reset();
  ChromeRenderViewHostTestHarness::TearDown();
}

std::unique_ptr<MockPasswordManagerDriver>
PasswordGenerationPopupControllerImplTest::CreateDriver() {
  return std::make_unique<MockPasswordManagerDriver>();
}

}  // namespace

TEST_F(PasswordGenerationPopupControllerImplTest, GetOrCreateTheSame) {
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller1 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data().bounds, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents(), main_rfh());

  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller2 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          controller1, ui_data().bounds, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents(), main_rfh());

  EXPECT_EQ(controller1.get(), controller2.get());
}

TEST_F(PasswordGenerationPopupControllerImplTest, GetOrCreateDifferentBounds) {
  gfx::RectF rect(100, 20);
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller1 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, rect, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents(), main_rfh());

  rect = gfx::RectF(200, 30);
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller2 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          controller1, rect, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents(), main_rfh());

  EXPECT_FALSE(controller1);
  EXPECT_TRUE(controller2);
}

TEST_F(PasswordGenerationPopupControllerImplTest, GetOrCreateDifferentTabs) {
  auto web_contents1 = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller1 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data().bounds, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents1.get(), main_rfh());

  auto web_contents2 = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller2 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          controller1, ui_data().bounds, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents2.get(), main_rfh());

  EXPECT_FALSE(controller1);
  EXPECT_TRUE(controller2);
}

TEST_F(PasswordGenerationPopupControllerImplTest, GetOrCreateDifferentDrivers) {
  auto driver1 = CreateDriver();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller1 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data().bounds, ui_data(),
          driver1->AsWeakPtr(),
          /*observer=*/nullptr, web_contents(), main_rfh());

  auto driver2 = CreateDriver();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller2 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          controller1, ui_data().bounds, ui_data(), driver2->AsWeakPtr(),
          /*observer=*/nullptr, web_contents(), main_rfh());

  EXPECT_FALSE(controller1);
  EXPECT_TRUE(controller2);
}

TEST_F(PasswordGenerationPopupControllerImplTest,
       GetOrCreateDifferentElements) {
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller1 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data().bounds, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents(), main_rfh());

  ui_data().generation_element_id = autofill::FieldRendererId(200);
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller2 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          controller1, ui_data().bounds, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents(), main_rfh());

  EXPECT_FALSE(controller1);
  EXPECT_TRUE(controller2);
}

TEST_F(PasswordGenerationPopupControllerImplTest, DestroyInPasswordAccepted) {
  base::WeakPtr<PasswordGenerationPopupController> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data().bounds, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents(), main_rfh());

  // Destroying the controller in GeneratedPasswordAccepted() should not cause a
  // crash.
  EXPECT_CALL(driver(),
              GeneratedPasswordAccepted(_, autofill::FieldRendererId(100), _))
      .WillOnce([controller](auto, auto, auto) {
        controller->Hide(autofill::SuggestionHidingReason::kViewDestroyed);
      });
  controller->PasswordAccepted();
}

TEST_F(PasswordGenerationPopupControllerImplTest, GetElementTextDirection) {
  ui_data().text_direction = base::i18n::TextDirection::RIGHT_TO_LEFT;
  base::WeakPtr<PasswordGenerationPopupController> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data().bounds, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents(), main_rfh());

  ASSERT_TRUE(controller);
  EXPECT_EQ(controller->GetElementTextDirection(),
            base::i18n::TextDirection::RIGHT_TO_LEFT);
}

TEST_F(PasswordGenerationPopupControllerImplTest,
       PreviewIsTriggeredDuringGeneration) {
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data().bounds, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents(), main_rfh());
  controller->SetViewForTesting(popup_view());

  // In the offer generation state, suggestions are previewed on selection.
  controller->GeneratePasswordValue(PasswordGenerationType::kAutomatic);
  controller->Show(
      PasswordGenerationPopupController::GenerationUIState::kOfferGeneration);
  EXPECT_CALL(driver(), PreviewGenerationSuggestion);
  static_cast<PasswordGenerationPopupController*>(controller.get())
      ->SetSelected();
}

TEST_F(PasswordGenerationPopupControllerImplTest,
       PreviewIsTriggeredOnlyDuringOfferGeneration) {
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data().bounds, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents(), main_rfh());
  controller->SetViewForTesting(popup_view());

  // In the edit generated password state, no preview calls happen.
  controller->GeneratePasswordValue(PasswordGenerationType::kAutomatic);
  controller->Show(PasswordGenerationPopupController::GenerationUIState::
                       kEditGeneratedPassword);
  EXPECT_CALL(driver(), PreviewGenerationSuggestion).Times(0);
  static_cast<PasswordGenerationPopupController*>(controller.get())
      ->SetSelected();
}

TEST_F(PasswordGenerationPopupControllerImplTest, ClearsFormPreviewOnHide) {
  base::WeakPtr<PasswordGenerationPopupController> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data().bounds, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents(), main_rfh());

  EXPECT_CALL(driver(), ClearPreviewedForm());
  controller->Hide(autofill::SuggestionHidingReason::kViewDestroyed);
}

TEST_F(PasswordGenerationPopupControllerImplTest,
       SuggestedTextDefaultPasswordLength) {
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data().bounds, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents(), main_rfh());
  controller->SetViewForTesting(popup_view());

  controller->GeneratePasswordValue(PasswordGenerationType::kAutomatic);
  EXPECT_EQ(static_cast<PasswordGenerationPopupController*>(controller.get())
                ->SuggestedText(),
            l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_SUGGESTION_GPM));
}

TEST_F(PasswordGenerationPopupControllerImplTest,
       SuggestedTextShorterPasswordLength) {
  // Limit the max length of the password.
  ui_data().max_length = 10;

  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data().bounds, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents(), main_rfh());
  controller->SetViewForTesting(popup_view());

  controller->GeneratePasswordValue(PasswordGenerationType::kAutomatic);
  EXPECT_EQ(static_cast<PasswordGenerationPopupController*>(controller.get())
                ->SuggestedText(),
            l10n_util::GetStringUTF16(
                IDS_PASSWORD_GENERATION_SUGGESTION_GPM_WITHOUT_STRONG));
}

TEST_F(PasswordGenerationPopupControllerImplTest,
       AdvancesFieldFocusOnUseStrongPassword) {
  base::WeakPtr<PasswordGenerationPopupController> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data().bounds, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents(), main_rfh());

  EXPECT_CALL(driver(),
              GeneratedPasswordAccepted(_, autofill::FieldRendererId(100), _));
  EXPECT_CALL(driver(), FocusNextFieldAfterPasswords);
  controller->PasswordAccepted();
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(PasswordGenerationPopupControllerImplTest,
       PreviewsGeneratedPasswordOnShowInNudgePassword) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordGenerationSoftNudge);

  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data().bounds, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents(), main_rfh());

  controller->SetViewForTesting(popup_view());
  // TODO(crbug.com/40267532): Rewrite controller_->Show() function to allow
  // testing expectations when the view doesn't exist.  SetViewForTesting
  // prevents that currently, hence the update view flow is being called.
  ON_CALL(*popup_view(), UpdateBoundsAndRedrawPopup)
      .WillByDefault(Return(true));

  // In the nudge password experiment suggestion is previewed on show.
  controller->GeneratePasswordValue(PasswordGenerationType::kAutomatic);
  EXPECT_CALL(driver(), PreviewGenerationSuggestion);
  controller->Show(
      PasswordGenerationPopupController::GenerationUIState::kOfferGeneration);
}

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace password_manager
