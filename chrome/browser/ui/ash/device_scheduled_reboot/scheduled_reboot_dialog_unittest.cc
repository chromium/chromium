// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <memory>

#include "chrome/browser/ui/ash/device_scheduled_reboot/scheduled_reboot_dialog.h"
#include "chrome/browser/ui/views/chrome_constrained_window_views_client.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/test/browser_task_environment.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

class ScheduledRebootDialogTest : public views::ViewsTestBase {
 public:
  ScheduledRebootDialogTest()
      : views::ViewsTestBase(std::unique_ptr<base::test::TaskEnvironment>(
            std::make_unique<content::BrowserTaskEnvironment>(
                content::BrowserTaskEnvironment::MainThreadType::UI,
                content::BrowserTaskEnvironment::TimeSource::MOCK_TIME))) {}
  ~ScheduledRebootDialogTest() override {}

  void SetUp() override {
    views::ViewsTestBase::SetUp();
    SetConstrainedWindowViewsClient(CreateChromeConstrainedWindowViewsClient());
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                     views::Widget::InitParams::TYPE_WINDOW);
    parent_widget_.Init(std::move(params));
    parent_widget_.Show();
  }

  void TearDown() override {
    parent_widget_.Close();
    dialog_.reset();
    constrained_window::SetConstrainedWindowViewsClient(nullptr);
    views::ViewsTestBase::TearDown();
  }

  void CreateViewAndShow();
  ScheduledRebootDialog* dialog() { return dialog_.get(); }

 private:
  views::Widget parent_widget_;
  std::unique_ptr<ScheduledRebootDialog> dialog_;
};

void ScheduledRebootDialogTest::CreateViewAndShow() {
  base::Time deadline = base::Time::Now() + base::Minutes(5);
  dialog_ = std::make_unique<ScheduledRebootDialog>(
      deadline, parent_widget_.GetNativeView(), base::DoNothing());
  views::DialogDelegate* dialog_model = dialog_->GetDialogDelegate();
  EXPECT_NE(dialog_model, nullptr);
  views::test::WidgetVisibleWaiter(dialog_model->GetWidget()).Wait();
  EXPECT_TRUE(dialog_model->GetWidget()->IsVisible());
}

TEST_F(ScheduledRebootDialogTest, VerifyWindowTitleChange) {
  CreateViewAndShow();
  // Initial title.
  views::DialogDelegate* delegate = dialog()->GetDialogDelegate();
  EXPECT_EQ(delegate->GetWindowTitle(),
            u"Your device will restart in 5 minutes");
  // Fast forward time by 2 minutes and verify the title has changed
  // accordingly.
  task_environment()->FastForwardBy(base::Minutes(2));
  EXPECT_EQ(delegate->GetWindowTitle(),
            u"Your device will restart in 3 minutes");
  // Fast forward time by 125 seconds and verify the title shows seconds until
  // expiration.
  task_environment()->FastForwardBy(base::Seconds(125));
  EXPECT_EQ(delegate->GetWindowTitle(),
            u"Your device will restart in 55 seconds");
  // Fast forward time to reboot time.
  task_environment()->FastForwardBy(base::Seconds(55));
  EXPECT_EQ(delegate->GetWindowTitle(), u"Your device will restart now");
}

TEST_F(ScheduledRebootDialogTest, CloseDialog) {
  CreateViewAndShow();
  // Initial title.
  views::DialogDelegate* delegate = dialog()->GetDialogDelegate();
  EXPECT_EQ(delegate->GetWindowTitle(),
            u"Your device will restart in 5 minutes");
  delegate->GetWidget()->CloseNow();
  // Expect dialog delegate is nullptr after closing the dialog.
  EXPECT_EQ(dialog()->GetDialogDelegate(), nullptr);
}
