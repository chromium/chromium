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
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/ui/popup_types.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
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

using autofill::password_generation::PasswordGenerationUIData;
using ::testing::_;
using ::testing::Return;

#if !BUILDFLAG(IS_ANDROID)
using password_manager::features::kPasswordGenerationExperiment;
using password_manager::prefs::kPasswordGenerationNudgePasswordDismissCount;
#endif  // !BUILDFLAG(IS_ANDROID)

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
  MOCK_METHOD(void, EditPasswordSelectionUpdated, (), (override));
};

class PasswordGenerationPopupControllerImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  std::unique_ptr<MockPasswordManagerDriver> CreateDriver();

 protected:
  MockPasswordGenerationPopupView* popup_view() { return &view_; }

 private:
  MockPasswordGenerationPopupView view_;
};

std::unique_ptr<MockPasswordManagerDriver>
PasswordGenerationPopupControllerImplTest::CreateDriver() {
  return std::make_unique<MockPasswordManagerDriver>();
}

}  // namespace

TEST_F(PasswordGenerationPopupControllerImplTest, GetOrCreateTheSame) {
  PasswordGenerationUIData ui_data = CreatePasswordGenerationUIData();
  auto driver = CreateDriver();
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller1 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data.bounds, ui_data, driver->AsWeakPtr(),
          /*observer=*/nullptr, web_contents.get(), main_rfh(),
          /*pref_service=*/nullptr);

  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller2 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          controller1, ui_data.bounds, ui_data, driver->AsWeakPtr(),
          /*observer=*/nullptr, web_contents.get(), main_rfh(),
          /*pref_service=*/nullptr);

  EXPECT_EQ(controller1.get(), controller2.get());
}

TEST_F(PasswordGenerationPopupControllerImplTest, GetOrCreateDifferentBounds) {
  gfx::RectF rect(100, 20);
  PasswordGenerationUIData ui_data = CreatePasswordGenerationUIData();
  auto driver = CreateDriver();
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller1 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, rect, ui_data, driver->AsWeakPtr(),
          /*observer=*/nullptr, web_contents.get(), main_rfh(),
          /*pref_service=*/nullptr);

  rect = gfx::RectF(200, 30);
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller2 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          controller1, rect, ui_data, driver->AsWeakPtr(), /*observer=*/nullptr,
          web_contents.get(), main_rfh(), /*pref_service=*/nullptr);

  EXPECT_FALSE(controller1);
  EXPECT_TRUE(controller2);
}

TEST_F(PasswordGenerationPopupControllerImplTest, GetOrCreateDifferentTabs) {
  PasswordGenerationUIData ui_data = CreatePasswordGenerationUIData();
  auto driver = CreateDriver();
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller1 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data.bounds, ui_data, driver->AsWeakPtr(),
          /*observer=*/nullptr, web_contents.get(), main_rfh(),
          /*pref_service=*/nullptr);

  web_contents = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller2 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          controller1, ui_data.bounds, ui_data, driver->AsWeakPtr(),
          /*observer=*/nullptr, web_contents.get(), main_rfh(),
          /*pref_service=*/nullptr);

  EXPECT_FALSE(controller1);
  EXPECT_TRUE(controller2);
}

TEST_F(PasswordGenerationPopupControllerImplTest, GetOrCreateDifferentDrivers) {
  PasswordGenerationUIData ui_data = CreatePasswordGenerationUIData();
  auto driver = CreateDriver();
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller1 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data.bounds, ui_data, driver->AsWeakPtr(),
          /*observer=*/nullptr, web_contents.get(), main_rfh(),
          /*pref_service=*/nullptr);

  driver = CreateDriver();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller2 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          controller1, ui_data.bounds, ui_data, driver->AsWeakPtr(),
          /*observer=*/nullptr, web_contents.get(), main_rfh(),
          /*pref_service=*/nullptr);

  EXPECT_FALSE(controller1);
  EXPECT_TRUE(controller2);
}

TEST_F(PasswordGenerationPopupControllerImplTest,
       GetOrCreateDifferentElements) {
  PasswordGenerationUIData ui_data = CreatePasswordGenerationUIData();
  auto driver = CreateDriver();
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller1 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data.bounds, ui_data, driver->AsWeakPtr(),
          /*observer=*/nullptr, web_contents.get(), main_rfh(),
          /*pref_service=*/nullptr);

  ui_data.generation_element_id = autofill::FieldRendererId(200);
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller2 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          controller1, ui_data.bounds, ui_data, driver->AsWeakPtr(),
          /*observer=*/nullptr, web_contents.get(), main_rfh(),
          /*pref_service=*/nullptr);

  EXPECT_FALSE(controller1);
  EXPECT_TRUE(controller2);
}

TEST_F(PasswordGenerationPopupControllerImplTest, DestroyInPasswordAccepted) {
  PasswordGenerationUIData ui_data = CreatePasswordGenerationUIData();
  auto driver = CreateDriver();
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupController> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data.bounds, ui_data, driver->AsWeakPtr(),
          /*observer=*/nullptr, web_contents.get(), main_rfh(),
          /*pref_service=*/nullptr);

  // Destroying the controller in GeneratedPasswordAccepted() should not cause a
  // crash.
  EXPECT_CALL(*driver,
              GeneratedPasswordAccepted(_, autofill::FieldRendererId(100), _))
      .WillOnce([controller](auto, auto, auto) {
        controller->Hide(autofill::PopupHidingReason::kViewDestroyed);
      });
  controller->PasswordAccepted();
}

TEST_F(PasswordGenerationPopupControllerImplTest, GetElementTextDirection) {
  PasswordGenerationUIData ui_data = CreatePasswordGenerationUIData();
  ui_data.text_direction = base::i18n::TextDirection::RIGHT_TO_LEFT;
  auto driver = CreateDriver();
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupController> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data.bounds, ui_data, driver->AsWeakPtr(),
          /*observer=*/nullptr, web_contents.get(), main_rfh(),
          /*pref_service=*/nullptr);

  ASSERT_TRUE(controller);
  EXPECT_EQ(controller->GetElementTextDirection(),
            base::i18n::TextDirection::RIGHT_TO_LEFT);
}

TEST_F(PasswordGenerationPopupControllerImplTest,
       PreviewIsTriggeredDuringGeneration) {
  // The password generation helper is needed in the offer generation state and
  // since the driver mock returns a raw pointer to it, we construct it first.
  StubPasswordManagerClient client;
  PasswordGenerationFrameHelper pw_generation_helper{&client,
                                                     /*driver=*/nullptr};

  PasswordGenerationUIData ui_data = CreatePasswordGenerationUIData();
  auto driver = CreateDriver();
  ON_CALL(*driver, GetPasswordGenerationHelper)
      .WillByDefault(Return(&pw_generation_helper));
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data.bounds, ui_data, driver->AsWeakPtr(),
          /*observer=*/nullptr, web_contents.get(), main_rfh(),
          /*pref_service=*/nullptr);
  controller->SetViewForTesting(popup_view());

  // In the offer generation state, suggestions are previewed on selection.
  controller->Show(
      PasswordGenerationPopupController::GenerationUIState::kOfferGeneration);
  EXPECT_CALL(*driver, PreviewGenerationSuggestion);
  static_cast<PasswordGenerationPopupController*>(controller.get())
      ->SetSelected();
}

TEST_F(PasswordGenerationPopupControllerImplTest,
       PreviewIsTriggeredOnlyDuringOfferGeneration) {
  PasswordGenerationUIData ui_data = CreatePasswordGenerationUIData();
  auto driver = CreateDriver();
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data.bounds, ui_data, driver->AsWeakPtr(),
          /*observer=*/nullptr, web_contents.get(), main_rfh(),
          /*pref_service=*/nullptr);
  controller->SetViewForTesting(popup_view());

  // In the edit generated password state, no preview calls happen.
  controller->Show(PasswordGenerationPopupController::GenerationUIState::
                       kEditGeneratedPassword);
  EXPECT_CALL(*driver, PreviewGenerationSuggestion).Times(0);
  static_cast<PasswordGenerationPopupController*>(controller.get())
      ->SetSelected();
}

TEST_F(PasswordGenerationPopupControllerImplTest, ClearsFormPreviewOnHide) {
  PasswordGenerationUIData ui_data = CreatePasswordGenerationUIData();
  auto driver = CreateDriver();
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupController> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data.bounds, ui_data, driver->AsWeakPtr(),
          /*observer=*/nullptr, web_contents.get(), main_rfh(),
          /*pref_service=*/nullptr);

  EXPECT_CALL(*driver, ClearPreviewedForm());
  controller->Hide(autofill::PopupHidingReason::kViewDestroyed);
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(PasswordGenerationPopupControllerImplTest,
       AdvancesFieldFocusOnUseStrongPassword) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kPasswordGenerationExperiment,
        {{"password_generation_variation", "edit_password"}}}},
      {});

  PasswordGenerationUIData ui_data{CreatePasswordGenerationUIData()};
  auto driver = CreateDriver();
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupController> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data.bounds, ui_data, driver->AsWeakPtr(),
          /*observer=*/nullptr, web_contents.get(), main_rfh(),
          /*pref_service=*/nullptr);

  EXPECT_CALL(*driver,
              GeneratedPasswordAccepted(_, autofill::FieldRendererId(100), _));
  EXPECT_CALL(*driver, FocusNextFieldAfterPasswords);
  controller->PasswordAccepted();
}

TEST_F(PasswordGenerationPopupControllerImplTest,
       DoesNotAdvanceFieldFocusOnEditPassword) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kPasswordGenerationExperiment,
        {{"password_generation_variation", "edit_password"}}}},
      {});

  PasswordGenerationUIData ui_data{CreatePasswordGenerationUIData()};
  auto driver = CreateDriver();
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupController> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data.bounds, ui_data, driver->AsWeakPtr(),
          /*observer=*/nullptr, web_contents.get(), main_rfh(),
          /*pref_service=*/nullptr);
  // EditPasswordClicked() below results in calling view->Show(), hence the need
  // to use the mock.
  static_cast<PasswordGenerationPopupControllerImpl*>(controller.get())
      ->SetViewForTesting(popup_view());

  EXPECT_CALL(*driver,
              GeneratedPasswordAccepted(_, autofill::FieldRendererId(100), _));
  EXPECT_CALL(*driver, FocusNextFieldAfterPasswords).Times(0);
  controller->EditPasswordClicked();
}

TEST_F(PasswordGenerationPopupControllerImplTest,
       PreviewsGeneratedPasswordOnShowInNudgePassword) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kPasswordGenerationExperiment,
        {{"password_generation_variation", "nudge_password"}}}},
      {});

  // The password generation helper is needed in the offer generation state and
  // since the driver mock returns a raw pointer to it, we construct it first.
  StubPasswordManagerClient client;
  PasswordGenerationFrameHelper pw_generation_helper{&client,
                                                     /*driver=*/nullptr};

  PasswordGenerationUIData ui_data = CreatePasswordGenerationUIData();
  auto driver = CreateDriver();
  ON_CALL(*driver, GetPasswordGenerationHelper)
      .WillByDefault(Return(&pw_generation_helper));
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data.bounds, ui_data, driver->AsWeakPtr(),
          /*observer=*/nullptr, web_contents.get(), main_rfh(),
          /*pref_service=*/nullptr);

  controller->SetViewForTesting(popup_view());
  // TODO(crbug.com/1444072): Rewrite controller_->Show() function to allow
  // testing expectations when the view doesn't exist.  SetViewForTesting
  // prevents that currently, hence the update view flow is being called.
  ON_CALL(*popup_view(), UpdateBoundsAndRedrawPopup)
      .WillByDefault(Return(true));

  // In the nudge password experiment suggestion is previewed on show.
  EXPECT_CALL(*driver, PreviewGenerationSuggestion);
  controller->Show(
      PasswordGenerationPopupController::GenerationUIState::kOfferGeneration);
}

TEST_F(PasswordGenerationPopupControllerImplTest,
       IncrementsNudgePasswordDismissCountPrefOnHide) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kPasswordGenerationExperiment,
        {{"password_generation_variation", "nudge_password"}}}},
      {});

  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterIntegerPref(
      kPasswordGenerationNudgePasswordDismissCount, 0);

  PasswordGenerationUIData ui_data = CreatePasswordGenerationUIData();
  auto driver = CreateDriver();
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupController> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data.bounds, ui_data, driver->AsWeakPtr(),
          /*observer=*/nullptr, web_contents.get(), main_rfh(), &pref_service);

  EXPECT_EQ(
      pref_service.GetInteger(kPasswordGenerationNudgePasswordDismissCount), 0);
  controller->Hide(autofill::PopupHidingReason::kUserAborted);
  EXPECT_EQ(
      pref_service.GetInteger(kPasswordGenerationNudgePasswordDismissCount), 1);
}

TEST_F(PasswordGenerationPopupControllerImplTest,
       ResetsNudgePasswordDismissCountPrefOnPasswordAccepted) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kPasswordGenerationExperiment,
        {{"password_generation_variation", "nudge_password"}}}},
      {});

  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterIntegerPref(
      kPasswordGenerationNudgePasswordDismissCount, 4);

  PasswordGenerationUIData ui_data = CreatePasswordGenerationUIData();
  auto driver = CreateDriver();
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupController> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data.bounds, ui_data, driver->AsWeakPtr(),
          /*observer=*/nullptr, web_contents.get(), main_rfh(), &pref_service);

  EXPECT_EQ(
      pref_service.GetInteger(kPasswordGenerationNudgePasswordDismissCount), 4);
  controller->PasswordAccepted();
  EXPECT_EQ(
      pref_service.GetInteger(kPasswordGenerationNudgePasswordDismissCount), 0);
}

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace password_manager
