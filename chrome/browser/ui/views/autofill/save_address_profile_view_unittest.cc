// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/save_address_profile_view.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using profile_ref = base::optional_ref<const AutofillProfile>;
using ::testing::Property;

class MockSaveAddressBubbleController : public SaveAddressBubbleController {
 public:
  MockSaveAddressBubbleController()
      : SaveAddressBubbleController(/*delegate=*/nullptr,
                                    /*web_contents=*/nullptr,
                                    /*address_profile=*/test::GetFullProfile(),
                                    /*is_migration_to_account=*/false) {}
  MockSaveAddressBubbleController(const MockSaveAddressBubbleController&) =
      delete;
  MockSaveAddressBubbleController& operator=(
      const MockSaveAddressBubbleController&) = delete;
  ~MockSaveAddressBubbleController() override = default;

  MOCK_METHOD(std::u16string, GetWindowTitle, (), (const, override));
  MOCK_METHOD(std::optional<HeaderImages>,
              GetHeaderImages,
              (),
              (const, override));
  MOCK_METHOD(std::u16string, GetBodyText, (), (const, override));
  MOCK_METHOD(std::u16string, GetAddressSummary, (), (const, override));
  MOCK_METHOD(std::u16string, GetProfileEmail, (), (const, override));
  MOCK_METHOD(std::u16string, GetProfilePhone, (), (const, override));
  MOCK_METHOD(std::u16string, GetOkButtonLabel, (), (const, override));
  MOCK_METHOD(AutofillClient::AddressPromptUserDecision,
              GetCancelCallbackValue,
              (),
              (const, override));
  MOCK_METHOD(std::u16string, GetFooterMessage, (), (const, override));
  MOCK_METHOD(void,
              OnUserDecision,
              (AutofillClient::AddressPromptUserDecision,
               base::optional_ref<const AutofillProfile>),
              (override));
  MOCK_METHOD(void, OnEditButtonClicked, (), (override));
  MOCK_METHOD(void, OnBubbleClosed, (), (override));
};

class SaveAddressProfileViewTest : public ChromeViewsTestBase {
 public:
  SaveAddressProfileViewTest() = default;
  ~SaveAddressProfileViewTest() override = default;

  std::unique_ptr<MockSaveAddressBubbleController> CreateViewController();
  void CreateViewAndShow(
      std::unique_ptr<MockSaveAddressBubbleController> controller);

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    address_profile_to_save_ = test::GetFullProfile();
    test_web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
  }

  void TearDown() override {
    std::exchange(view_, nullptr)
        ->GetWidget()
        ->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);
    anchor_widget_.reset();

    ChromeViewsTestBase::TearDown();
  }

  const AutofillProfile& address_profile_to_save() {
    return address_profile_to_save_;
  }
  SaveAddressProfileView* view() { return view_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  TestingProfile profile_;
  AutofillProfile address_profile_to_save_{
      i18n_model_definition::kLegacyHierarchyCountryCode};
  // This enables uses of TestWebContents.
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<content::WebContents> test_web_contents_;
  std::unique_ptr<views::Widget> anchor_widget_;
  raw_ptr<SaveAddressProfileView> view_ = nullptr;
};

std::unique_ptr<MockSaveAddressBubbleController>
SaveAddressProfileViewTest::CreateViewController() {
  auto mock_controller =
      std::make_unique<testing::NiceMock<MockSaveAddressBubbleController>>();

  ON_CALL(*mock_controller, GetCancelCallbackValue)
      .WillByDefault(::testing::Return(
          AutofillClient::AddressPromptUserDecision::kDeclined));
  ON_CALL(*mock_controller, GetWindowTitle())
      .WillByDefault(testing::Return(std::u16string()));

  return mock_controller;
}

void SaveAddressProfileViewTest::CreateViewAndShow(
    std::unique_ptr<MockSaveAddressBubbleController> controller) {
  // The bubble needs the parent as an anchor.
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                   views::Widget::InitParams::TYPE_WINDOW);

  anchor_widget_ = std::make_unique<views::Widget>();
  anchor_widget_->Init(std::move(params));
  anchor_widget_->Show();

  view_ = new SaveAddressProfileView(std::move(controller),
                                     anchor_widget_->GetContentsView(),
                                     test_web_contents_.get());
  views::BubbleDialogDelegateView::CreateBubble(view_)->Show();
}

TEST_F(SaveAddressProfileViewTest, HasCloseButton) {
  CreateViewAndShow(CreateViewController());
  EXPECT_TRUE(view()->ShouldShowCloseButton());
}

TEST_F(SaveAddressProfileViewTest, AcceptInvokesTheController) {
  std::unique_ptr<MockSaveAddressBubbleController> controller =
      CreateViewController();
  EXPECT_CALL(
      *controller,
      OnUserDecision(AutofillClient::AddressPromptUserDecision::kAccepted,
                     Property(&profile_ref::has_value, false)));
  CreateViewAndShow(std::move(controller));
  view()->AcceptDialog();
}

TEST_F(SaveAddressProfileViewTest, CancelInvokesTheController) {
  std::unique_ptr<MockSaveAddressBubbleController> controller =
      CreateViewController();
  EXPECT_CALL(
      *controller,
      OnUserDecision(AutofillClient::AddressPromptUserDecision::kDeclined,
                     Property(&profile_ref::has_value, false)));
  CreateViewAndShow(std::move(controller));
  view()->CancelDialog();
}

}  // namespace autofill
