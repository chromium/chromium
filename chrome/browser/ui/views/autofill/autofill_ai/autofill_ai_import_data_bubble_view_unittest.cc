// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_ai/autofill_ai_import_data_bubble_view.h"

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_ai/autofill_ai_import_data_controller.h"
#include "chrome/browser/ui/autofill/autofill_ai/mock_autofill_ai_import_data_controller.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/widget/widget_utils.h"

// TODO(crbug.com/362227379): Consider having an interactive UI test to evaluate
// both the controller and the view working together.
namespace autofill {

namespace {

using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;

class AutofillAiImportDataBubbleViewTest : public ChromeViewsTestBase {
 public:
  AutofillAiImportDataBubbleViewTest() = default;
  ~AutofillAiImportDataBubbleViewTest() override = default;

  // views::ViewsTestBase:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
  }

  void CreateViewAndShow();

  void TearDown() override {
    ResetViewPointer();
    anchor_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  AutofillAiImportDataBubbleView* view() { return view_.get(); }
  MockAutofillAiImportDataController& mock_controller() {
    return mock_controller_;
  }

  void ResetViewPointer() {
    view_ = nullptr;
  }

 private:
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<views::Widget> anchor_widget_;
  raw_ptr<AutofillAiImportDataBubbleView> view_ = nullptr;
  NiceMock<MockAutofillAiImportDataController> mock_controller_;
};

void AutofillAiImportDataBubbleViewTest::CreateViewAndShow() {
  // The bubble needs the parent as an anchor.
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                   views::Widget::InitParams::TYPE_WINDOW);
  anchor_widget_ = std::make_unique<views::Widget>();
  anchor_widget_->Init(std::move(params));
  anchor_widget_->Show();
  std::vector<EntityAttributeUpdateDetails> details = {
      // The first two values are updates done by the user. Specifically
      // they updated their name and added country data.
      EntityAttributeUpdateDetails(
          /*attribute_name=*/u"Name", /*attribute_value=*/u"Jon doe",
          /*old_attribute_value=*/u"Seb doe",
          EntityAttributeUpdateType::kNewEntityAttributeUpdated),
      EntityAttributeUpdateDetails(
          /*attribute_name=*/u"Country", /*attribute_value=*/u"Brazil",
          /*old_attribute_value=*/u"Ukraine",
          EntityAttributeUpdateType::kNewEntityAttributeAdded),
      // The next two values are saying:
      // 1. That the user's passport expiry date information has not changed.
      // 2. That the user's passport issue date information has not changed.
      EntityAttributeUpdateDetails(
          /*attribute_name=*/u"Expiry date",
          /*attribute_value=*/u"12/12/2027",
          /*old_attribute_value=*/std::nullopt,
          EntityAttributeUpdateType::kNewEntityAttributeUnchanged),
      EntityAttributeUpdateDetails(
          /*attribute_name=*/u"Issue date",
          /*attribute_value=*/u"12/12/2020",
          /*old_attribute_value=*/std::nullopt,
          EntityAttributeUpdateType::kNewEntityAttributeUnchanged)};
  ON_CALL(mock_controller(), GetUpdatedAttributesDetails())
      .WillByDefault(Return(details));
  ON_CALL(mock_controller(), CloseOnAccept()).WillByDefault(Return(true));

  auto view_unique = std::make_unique<AutofillAiImportDataBubbleView>(
      anchor_widget_->GetContentsView(), web_contents_.get(),
      &mock_controller_);
  view_ = view_unique.get();
  views::BubbleDialogDelegateView::CreateBubble(std::move(view_unique))->Show();
}

TEST_F(AutofillAiImportDataBubbleViewTest, HasCloseButton) {
  CreateViewAndShow();
  EXPECT_TRUE(view()->ShouldShowCloseButton());
}

TEST_F(AutofillAiImportDataBubbleViewTest, AcceptInvokesTheController) {
  CreateViewAndShow();

  base::RunLoop run_loop;
  EXPECT_CALL(mock_controller(), OnSaveButtonClicked);
  EXPECT_CALL(mock_controller(), OnBubbleClosed).WillOnce([this, &run_loop]() {
    ResetViewPointer();
    run_loop.Quit();
  });

  view()->AcceptDialog();
  run_loop.Run();
}

// Tests that the bubble is not closed when the controller does not want to
// close it on accept.
TEST_F(AutofillAiImportDataBubbleViewTest, AcceptDoesNotCloseTheBubble) {
  CreateViewAndShow();
  EXPECT_CALL(mock_controller(), CloseOnAccept()).WillOnce(Return(false));
  EXPECT_CALL(mock_controller(), OnSaveButtonClicked);
  EXPECT_CALL(mock_controller(), OnBubbleClosed).Times(0);
  view()->AcceptDialog();

  task_environment()->RunUntilIdle();
  // Clear expectations explicitly since the widget is destroyed during tear
  // down.
  Mock::VerifyAndClearExpectations(&mock_controller());
}

TEST_F(AutofillAiImportDataBubbleViewTest, AcceptShowsThrobber) {
  CreateViewAndShow();
  EXPECT_CALL(mock_controller(), CloseOnAccept()).WillOnce(Return(false));
  EXPECT_CALL(mock_controller(), OnSaveButtonClicked);

  views::View* throbber = view()->GetViewByID(DialogViewId::LOADING_THROBBER);
  ASSERT_TRUE(throbber);
  EXPECT_FALSE(throbber->parent()->GetVisible());

  view()->AcceptDialog();

  EXPECT_TRUE(throbber->parent()->GetVisible());
  EXPECT_EQ(view()->buttons(),
            static_cast<int>(ui::mojom::DialogButton::kNone));
}

TEST_F(AutofillAiImportDataBubbleViewTest, CancelInvokesTheController) {
  CreateViewAndShow();

  base::RunLoop run_loop;
  EXPECT_CALL(mock_controller(), OnBubbleClosed).WillOnce([this, &run_loop]() {
    ResetViewPointer();
    run_loop.Quit();
  });

  view()->CancelDialog();
  run_loop.Run();
}

}  // namespace

}  // namespace autofill
