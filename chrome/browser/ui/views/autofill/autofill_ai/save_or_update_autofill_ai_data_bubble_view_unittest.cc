// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_ai/save_or_update_autofill_ai_data_bubble_view.h"

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_ai/mock_save_or_update_ai_data_controller.h"
#include "chrome/browser/ui/autofill/autofill_ai/save_or_update_autofill_ai_data_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill_ai/core/browser/autofill_ai_client.h"
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
namespace autofill_ai {

namespace {

using EntityAttributeUpdateDetails =
    SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateDetails;
using EntityAttributeUpdateType =
    SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateType;
class SaveOrUpdateAutofillAiDataBubbleViewTest : public ChromeViewsTestBase {
 public:
  SaveOrUpdateAutofillAiDataBubbleViewTest() = default;
  ~SaveOrUpdateAutofillAiDataBubbleViewTest() override = default;

  // views::ViewsTestBase:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
  }

  void CreateViewAndShow();

  void TearDown() override {
    view_ = nullptr;
    anchor_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  SaveOrUpdateAutofillAiDataBubbleView& view() { return *view_; }
  MockSaveOrUpdateAutofillAiDataController& mock_controller() {
    return mock_controller_;
  }

  void ClickButton(views::ImageButton* button) {
    CHECK(button);
    ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), 0, 0);
    views::test::ButtonTestApi test_api(button);
    test_api.NotifyClick(e);
  }

 private:
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<views::Widget> anchor_widget_;
  raw_ptr<SaveOrUpdateAutofillAiDataBubbleView> view_ = nullptr;
  testing::NiceMock<MockSaveOrUpdateAutofillAiDataController> mock_controller_;
};

void SaveOrUpdateAutofillAiDataBubbleViewTest::CreateViewAndShow() {
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
          EntityAttributeUpdateType::kNewEntityAttributeUpdated),
      EntityAttributeUpdateDetails(
          /*attribute_name=*/u"Country", /*attribute_value=*/u"Brazil",
          EntityAttributeUpdateType::kNewEntityAttributeAdded),
      // The next two values are saying:
      // 1. That the user's passport expiry date information has not changed.
      // 2. That the user's passport issue date information has not changed.
      EntityAttributeUpdateDetails(
          /*attribute_name=*/u"Expiry date",
          /*attribute_value=*/u"12/12/2027",
          EntityAttributeUpdateType::kNewEntityAttributeUnchanged),
      EntityAttributeUpdateDetails(
          /*attribute_name=*/u"Issue date",
          /*attribute_value=*/u"12/12/2020",
          EntityAttributeUpdateType::kNewEntityAttributeUnchanged)};
  ON_CALL(mock_controller(), GetUpdatedAttributesDetails())
      .WillByDefault(testing::Return(details));

  auto view_unique = std::make_unique<SaveOrUpdateAutofillAiDataBubbleView>(
      anchor_widget_->GetContentsView(), web_contents_.get(),
      &mock_controller_);
  view_ = view_unique.get();
  views::BubbleDialogDelegateView::CreateBubble(std::move(view_unique))->Show();
}

TEST_F(SaveOrUpdateAutofillAiDataBubbleViewTest, HasCloseButton) {
  CreateViewAndShow();
  EXPECT_TRUE(view().ShouldShowCloseButton());
}

TEST_F(SaveOrUpdateAutofillAiDataBubbleViewTest, AcceptInvokesTheController) {
  CreateViewAndShow();
  EXPECT_CALL(mock_controller(), OnSaveButtonClicked);
  view().AcceptDialog();
}

TEST_F(SaveOrUpdateAutofillAiDataBubbleViewTest, CancelInvokesTheController) {
  CreateViewAndShow();
  EXPECT_CALL(mock_controller(), OnBubbleClosed);
  view().CancelDialog();
}

}  // namespace

}  // namespace autofill_ai
