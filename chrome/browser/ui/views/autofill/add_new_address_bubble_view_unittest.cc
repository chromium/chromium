// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/add_new_address_bubble_view.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class MockAddNewAddressBubbleController : public AddNewAddressBubbleController {
 public:
  explicit MockAddNewAddressBubbleController(content::WebContents* web_contents)
      : AddNewAddressBubbleController(web_contents,
                                      /*delegate=*/nullptr) {}
  MOCK_METHOD(std::u16string, GetBodyText, (), (const, override));
  MOCK_METHOD(std::u16string, GetFooterMessage, (), (const, override));
  MOCK_METHOD(void,
              OnUserDecision,
              (AutofillClient::AddressPromptUserDecision),
              (override));
  MOCK_METHOD(void, OnAddButtonClicked, (), (override));
  MOCK_METHOD(void, OnBubbleClosed, (), (override));
};

class AddNewAddressBubbleViewTest : public ChromeViewsTestBase {
 public:
  AddNewAddressBubbleViewTest() = default;
  ~AddNewAddressBubbleViewTest() override = default;

  void CreateViewAndShow(const std::u16string& footer_text = u"");

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    test_web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    autofill_client_injector_[test_web_contents_.get()]
        ->GetPersonalDataManager()
        ->test_address_data_manager()
        .SetAutofillProfileEnabled(true);
  }

  void TearDown() override {
    mock_controller_ = nullptr;
    std::exchange(view_, nullptr)
        ->GetWidget()
        ->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);
    anchor_widget_.reset();

    ChromeViewsTestBase::TearDown();
  }

  AddNewAddressBubbleView* view() { return view_; }
  MockAddNewAddressBubbleController* mock_controller() {
    return mock_controller_;
  }

 private:
  TestingProfile profile_;

  // This enables uses of TestWebContents.
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<content::WebContents> test_web_contents_;
  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
  std::unique_ptr<views::Widget> anchor_widget_;
  raw_ptr<AddNewAddressBubbleView> view_ = nullptr;
  raw_ptr<testing::NiceMock<MockAddNewAddressBubbleController>>
      mock_controller_ = nullptr;
};

void AddNewAddressBubbleViewTest::CreateViewAndShow(
    const std::u16string& footer_text) {
  // The bubble needs the parent as an anchor.
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                   views::Widget::InitParams::TYPE_WINDOW);

  anchor_widget_ = std::make_unique<views::Widget>();
  anchor_widget_->Init(std::move(params));
  anchor_widget_->Show();

  auto mock_controller_unique =
      std::make_unique<testing::NiceMock<MockAddNewAddressBubbleController>>(
          test_web_contents_.get());
  mock_controller_ = mock_controller_unique.get();

  ON_CALL(*mock_controller(), GetBodyText())
      .WillByDefault(testing::Return(std::u16string()));
  ON_CALL(*mock_controller(), GetFooterMessage())
      .WillByDefault(testing::Return(footer_text));

  view_ = new AddNewAddressBubbleView(std::move(mock_controller_unique),
                                      anchor_widget_->GetContentsView(),
                                      test_web_contents_.get());
  views::BubbleDialogDelegateView::CreateBubble(view_)->Show();
}

TEST_F(AddNewAddressBubbleViewTest, HasCloseButton) {
  CreateViewAndShow();
  EXPECT_TRUE(view()->ShouldShowCloseButton());
}

TEST_F(AddNewAddressBubbleViewTest, AcceptInvokesTheController) {
  CreateViewAndShow();
  EXPECT_CALL(*mock_controller(), OnAddButtonClicked);
  view()->AcceptDialog();
}

TEST_F(AddNewAddressBubbleViewTest, CancelInvokesTheController) {
  CreateViewAndShow();
  EXPECT_CALL(
      *mock_controller(),
      OnUserDecision(AutofillClient::AddressPromptUserDecision::kDeclined));
  view()->CancelDialog();
}

TEST_F(AddNewAddressBubbleViewTest, NoFooterStringFromController) {
  CreateViewAndShow();
  EXPECT_EQ(view()->GetFootnoteViewForTesting(), nullptr);
}

TEST_F(AddNewAddressBubbleViewTest, FooterIsAddedWithStringFromController) {
  CreateViewAndShow(/*footer_text=*/u"Some footer text");
  EXPECT_NE(view()->GetFootnoteViewForTesting(), nullptr);
}
}  // namespace autofill
