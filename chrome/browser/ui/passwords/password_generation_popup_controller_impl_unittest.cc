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
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_f.h"

namespace password_manager {

namespace {

using autofill::password_generation::PasswordGenerationUIData;
using ::testing::_;
using ::testing::Return;

PasswordGenerationUIData CreatePasswordGenerationUIData() {
  return PasswordGenerationUIData(gfx::RectF(100, 20), /*max_length=*/20,
                                  u"element", autofill::FieldRendererId(100),
                                  /*is_generation_element_password_type=*/true,
                                  base::i18n::TextDirection(),
                                  autofill::FormData());
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
};

class MockPasswordGenerationPopupView : public PasswordGenerationPopupView {
  MOCK_METHOD(bool, Show, (), (override));
  MOCK_METHOD(void, Hide, (), (override));
  MOCK_METHOD(void, UpdateState, (), (override));
  MOCK_METHOD(void, UpdateGeneratedPasswordValue, (), (override));
  MOCK_METHOD(bool, UpdateBoundsAndRedrawPopup, (), (override));
  MOCK_METHOD(void, PasswordSelectionUpdated, (), (override));
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
  PasswordGenerationUIData ui_data{CreatePasswordGenerationUIData()};
  auto driver = CreateDriver();
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller1 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          nullptr, ui_data.bounds, ui_data, driver->AsWeakPtr(), nullptr,
          web_contents.get(), main_rfh());

  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller2 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          controller1, ui_data.bounds, ui_data, driver->AsWeakPtr(), nullptr,
          web_contents.get(), main_rfh());

  EXPECT_EQ(controller1.get(), controller2.get());
}

TEST_F(PasswordGenerationPopupControllerImplTest, GetOrCreateDifferentBounds) {
  gfx::RectF rect(100, 20);
  PasswordGenerationUIData ui_data{CreatePasswordGenerationUIData()};
  auto driver = CreateDriver();
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller1 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          nullptr, rect, ui_data, driver->AsWeakPtr(), nullptr,
          web_contents.get(), main_rfh());

  rect = gfx::RectF(200, 30);
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller2 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          controller1, rect, ui_data, driver->AsWeakPtr(), nullptr,
          web_contents.get(), main_rfh());

  EXPECT_FALSE(controller1);
  EXPECT_TRUE(controller2);
}

TEST_F(PasswordGenerationPopupControllerImplTest, GetOrCreateDifferentTabs) {
  PasswordGenerationUIData ui_data{CreatePasswordGenerationUIData()};
  auto driver = CreateDriver();
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller1 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          nullptr, ui_data.bounds, ui_data, driver->AsWeakPtr(), nullptr,
          web_contents.get(), main_rfh());

  web_contents = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller2 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          controller1, ui_data.bounds, ui_data, driver->AsWeakPtr(), nullptr,
          web_contents.get(), main_rfh());

  EXPECT_FALSE(controller1);
  EXPECT_TRUE(controller2);
}

TEST_F(PasswordGenerationPopupControllerImplTest, GetOrCreateDifferentDrivers) {
  PasswordGenerationUIData ui_data{CreatePasswordGenerationUIData()};
  auto driver = CreateDriver();
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller1 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          nullptr, ui_data.bounds, ui_data, driver->AsWeakPtr(), nullptr,
          web_contents.get(), main_rfh());

  driver = CreateDriver();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller2 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          controller1, ui_data.bounds, ui_data, driver->AsWeakPtr(), nullptr,
          web_contents.get(), main_rfh());

  EXPECT_FALSE(controller1);
  EXPECT_TRUE(controller2);
}

TEST_F(PasswordGenerationPopupControllerImplTest,
       GetOrCreateDifferentElements) {
  PasswordGenerationUIData ui_data{CreatePasswordGenerationUIData()};
  auto driver = CreateDriver();
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller1 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          nullptr, ui_data.bounds, ui_data, driver->AsWeakPtr(), nullptr,
          web_contents.get(), main_rfh());

  ui_data.generation_element_id = autofill::FieldRendererId(200);
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller2 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          controller1, ui_data.bounds, ui_data, driver->AsWeakPtr(), nullptr,
          web_contents.get(), main_rfh());

  EXPECT_FALSE(controller1);
  EXPECT_TRUE(controller2);
}

TEST_F(PasswordGenerationPopupControllerImplTest, DestroyInPasswordAccepted) {
  PasswordGenerationUIData ui_data{CreatePasswordGenerationUIData()};
  auto driver = CreateDriver();
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupController> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          nullptr /*previous*/, ui_data.bounds, ui_data, driver->AsWeakPtr(),
          nullptr, web_contents.get(), main_rfh());

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
  PasswordGenerationUIData ui_data{CreatePasswordGenerationUIData()};
  ui_data.text_direction = base::i18n::TextDirection::RIGHT_TO_LEFT;
  auto driver = CreateDriver();
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupController> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          nullptr /*previous*/, ui_data.bounds, ui_data, driver->AsWeakPtr(),
          nullptr, web_contents.get(), main_rfh());

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

  PasswordGenerationUIData ui_data{CreatePasswordGenerationUIData()};
  auto driver = CreateDriver();
  ON_CALL(*driver, GetPasswordGenerationHelper)
      .WillByDefault(Return(&pw_generation_helper));
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          nullptr /*previous*/, ui_data.bounds, ui_data, driver->AsWeakPtr(),
          nullptr, web_contents.get(), main_rfh());
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
  PasswordGenerationUIData ui_data{CreatePasswordGenerationUIData()};
  auto driver = CreateDriver();
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          nullptr /*previous*/, ui_data.bounds, ui_data, driver->AsWeakPtr(),
          nullptr, web_contents.get(), main_rfh());
  controller->SetViewForTesting(popup_view());

  // In the edit generated password state, no preview calls happen.
  controller->Show(PasswordGenerationPopupController::GenerationUIState::
                       kEditGeneratedPassword);
  EXPECT_CALL(*driver, PreviewGenerationSuggestion).Times(0);
  static_cast<PasswordGenerationPopupController*>(controller.get())
      ->SetSelected();
}

}  // namespace password_manager
