// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_ai/autofill_ai_local_save_notification_view.h"

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

namespace autofill {

namespace {

using ::testing::NiceMock;
using ::testing::Return;

class AutofillAiLocalSaveNotificationViewTest : public ChromeViewsTestBase {
 public:
  AutofillAiLocalSaveNotificationViewTest() = default;
  ~AutofillAiLocalSaveNotificationViewTest() override = default;

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

  MockAutofillAiImportDataController& mock_controller() {
    return mock_controller_;
  }
  AutofillAiLocalSaveNotificationView* view() { return view_.get(); }

  void ResetViewPointer() { view_ = nullptr; }

 private:
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<views::Widget> anchor_widget_;
  raw_ptr<AutofillAiLocalSaveNotificationView> view_ = nullptr;
  NiceMock<MockAutofillAiImportDataController> mock_controller_;
};

void AutofillAiLocalSaveNotificationViewTest::CreateViewAndShow() {
  // The bubble needs the parent as an anchor.
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                   views::Widget::InitParams::TYPE_WINDOW);
  anchor_widget_ = std::make_unique<views::Widget>();
  anchor_widget_->Init(std::move(params));
  anchor_widget_->Show();

  auto view_unique = std::make_unique<AutofillAiLocalSaveNotificationView>(
      anchor_widget_->GetContentsView(), web_contents_.get(),
      &mock_controller_);
  view_ = view_unique.get();
  views::BubbleDialogDelegateView::CreateBubble(std::move(view_unique))->Show();
}

TEST_F(AutofillAiLocalSaveNotificationViewTest, AcceptInformsTheController) {
  CreateViewAndShow();

  base::RunLoop run_loop;
  EXPECT_CALL(mock_controller(), OnBubbleClosed).WillOnce([this, &run_loop]() {
    ResetViewPointer();
    run_loop.Quit();
  });

  view()->AcceptDialog();
  run_loop.Run();
}

TEST_F(AutofillAiLocalSaveNotificationViewTest, CloseInformsTheController) {
  CreateViewAndShow();

  base::RunLoop run_loop;
  EXPECT_CALL(mock_controller(), OnBubbleClosed).WillOnce([this, &run_loop]() {
    ResetViewPointer();
    run_loop.Quit();
  });

  view()->Hide();
  run_loop.Run();
}

}  // namespace

}  // namespace autofill
