// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_generation_popup_controller_impl.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_f.h"

namespace {

using ::testing::_;

class MockPasswordManagerDriver
    : public password_manager::StubPasswordManagerDriver {
 public:
  MOCK_METHOD1(GeneratedPasswordAccepted, void(const std::u16string&));
  MOCK_METHOD3(GeneratedPasswordAccepted,
               void(const autofill::FormData&,
                    autofill::FieldRendererId,
                    const std::u16string&));
};

class PasswordGenerationPopupControllerImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  std::unique_ptr<MockPasswordManagerDriver> CreateDriver();
};

std::unique_ptr<MockPasswordManagerDriver>
PasswordGenerationPopupControllerImplTest::CreateDriver() {
  return std::make_unique<MockPasswordManagerDriver>();
}

TEST_F(PasswordGenerationPopupControllerImplTest, GetOrCreateTheSame) {
  autofill::password_generation::PasswordGenerationUIData ui_data(
      gfx::RectF(100, 20), /*max_length=*/20, u"element",
      /*user_typed_password=*/std::u16string(), autofill::FieldRendererId(100),
      /*is_generation_element_password_type=*/true, base::i18n::TextDirection(),
      autofill::FormData());
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
  autofill::password_generation::PasswordGenerationUIData ui_data(
      rect, /*max_length=*/20, u"element",
      /*user_typed_password=*/std::u16string(), autofill::FieldRendererId(100),
      /*is_generation_element_password_type=*/true, base::i18n::TextDirection(),
      autofill::FormData());
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
  autofill::password_generation::PasswordGenerationUIData ui_data(
      gfx::RectF(100, 20), /*max_length=*/20, u"element",
      /*user_typed_password=*/std::u16string(), autofill::FieldRendererId(100),
      /*is_generation_element_password_type=*/true, base::i18n::TextDirection(),
      autofill::FormData());
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
  autofill::password_generation::PasswordGenerationUIData ui_data(
      gfx::RectF(100, 20), /*max_length=*/20, u"element",
      /*user_typed_password=*/std::u16string(), autofill::FieldRendererId(100),
      /*is_generation_element_password_type=*/true, base::i18n::TextDirection(),
      autofill::FormData());
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
  autofill::password_generation::PasswordGenerationUIData ui_data(
      gfx::RectF(100, 20), /*max_length=*/20, u"element",
      /*user_typed_password=*/std::u16string(), autofill::FieldRendererId(100),
      /*is_generation_element_password_type=*/true, base::i18n::TextDirection(),
      autofill::FormData());
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
  autofill::password_generation::PasswordGenerationUIData ui_data(
      gfx::RectF(100, 20), /*max_length=*/20, u"element",
      /*user_typed_password=*/std::u16string(), autofill::FieldRendererId(100),
      /*is_generation_element_password_type=*/true, base::i18n::TextDirection(),
      autofill::FormData());
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

}  // namespace
