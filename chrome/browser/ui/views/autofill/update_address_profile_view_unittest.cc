// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/update_address_profile_view.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/autofill/save_update_address_profile_bubble_controller.h"
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

class MockSaveUpdateAddressProfileBubbleController
    : public SaveUpdateAddressProfileBubbleController {
 public:
  MOCK_METHOD(std::u16string, GetWindowTitle, (), (const, override));
  MOCK_METHOD(absl::optional<HeaderImages>,
              GetHeaderImages,
              (),
              (const, override));
  MOCK_METHOD(std::u16string, GetBodyText, (), (const, override));
  MOCK_METHOD(std::u16string, GetAddressSummary, (), (const, override));
  MOCK_METHOD(std::u16string, GetProfileEmail, (), (const, override));
  MOCK_METHOD(std::u16string, GetProfilePhone, (), (const, override));
  MOCK_METHOD(std::u16string, GetOkButtonLabel, (), (const, override));
  MOCK_METHOD(AutofillClient::SaveAddressProfileOfferUserDecision,
              GetCancelCallbackValue,
              (),
              (const, override));
  MOCK_METHOD(std::u16string, GetFooterMessage, (), (const, override));
  MOCK_METHOD(const AutofillProfile&, GetProfileToSave, (), (const, override));
  MOCK_METHOD(const AutofillProfile*,
              GetOriginalProfile,
              (),
              (const, override));
  MOCK_METHOD(void,
              OnUserDecision,
              (AutofillClient::SaveAddressProfileOfferUserDecision decision),
              (override));
  MOCK_METHOD(void, OnEditButtonClicked, (), (override));
  MOCK_METHOD(void, OnBubbleClosed, (), (override));
};

class UpdateAddressProfileViewTest : public ChromeViewsTestBase {
 public:
  UpdateAddressProfileViewTest() = default;
  ~UpdateAddressProfileViewTest() override = default;

  void CreateViewAndShow();

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    address_profile_to_save_ = test::GetFullProfile();
    test_web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
  }

  void TearDown() override {
    view_->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kCloseButtonClicked);
    anchor_widget_.reset();

    ChromeViewsTestBase::TearDown();
  }

  const AutofillProfile& address_profile_to_save() {
    return address_profile_to_save_;
  }
  const AutofillProfile* original_profile() { return &original_profile_; }
  UpdateAddressProfileView* view() { return view_; }
  MockSaveUpdateAddressProfileBubbleController* mock_controller() {
    return &mock_controller_;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  TestingProfile profile_;

  AutofillProfile address_profile_to_save_ = test::GetFullProfile();
  AutofillProfile original_profile_ = test::GetFullProfile2();

  // This enables uses of TestWebContents.
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<content::WebContents> test_web_contents_;
  std::unique_ptr<views::Widget> anchor_widget_;
  raw_ptr<UpdateAddressProfileView, DanglingUntriaged> view_;
  testing::NiceMock<MockSaveUpdateAddressProfileBubbleController>
      mock_controller_;
};

void UpdateAddressProfileViewTest::CreateViewAndShow() {
  ON_CALL(*mock_controller(), GetWindowTitle())
      .WillByDefault(testing::Return(std::u16string()));
  ON_CALL(*mock_controller(), GetProfileToSave())
      .WillByDefault(testing::ReturnRef(address_profile_to_save()));
  ON_CALL(*mock_controller(), GetOriginalProfile())
      .WillByDefault(testing::Return(original_profile()));

  // The bubble needs the parent as an anchor.
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;

  anchor_widget_ = std::make_unique<views::Widget>();
  anchor_widget_->Init(std::move(params));
  anchor_widget_->Show();

  view_ =
      new UpdateAddressProfileView(anchor_widget_->GetContentsView(),
                                   test_web_contents_.get(), mock_controller());
  views::BubbleDialogDelegateView::CreateBubble(view_)->Show();
}

TEST_F(UpdateAddressProfileViewTest, HasCloseButton) {
  CreateViewAndShow();
  EXPECT_TRUE(view()->ShouldShowCloseButton());
}

TEST_F(UpdateAddressProfileViewTest, AcceptInvokesTheController) {
  CreateViewAndShow();
  EXPECT_CALL(
      *mock_controller(),
      OnUserDecision(
          AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted));
  view()->AcceptDialog();
}

TEST_F(UpdateAddressProfileViewTest, CancelInvokesTheController) {
  CreateViewAndShow();
  EXPECT_CALL(
      *mock_controller(),
      OnUserDecision(
          AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined));
  view()->CancelDialog();
}

}  // namespace autofill
