// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_security_view.h"

#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_view.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/download/public/common/mock_download_item.h"
#include "content/public/test/mock_download_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"

namespace {

using WarningSurface = DownloadItemWarningData::WarningSurface;
using WarningAction = DownloadItemWarningData::WarningAction;
using WarningActionEvent = DownloadItemWarningData::WarningActionEvent;

class MockDownloadBubbleUIController : public DownloadBubbleUIController {
 public:
  explicit MockDownloadBubbleUIController(Browser* browser)
      : DownloadBubbleUIController(browser) {}
  ~MockDownloadBubbleUIController() = default;
};

class MockDownloadBubbleNavigationHandler
    : public DownloadBubbleNavigationHandler {
 public:
  virtual ~MockDownloadBubbleNavigationHandler() = default;
  void OpenPrimaryDialog() override {}
  void OpenSecurityDialog(DownloadBubbleRowView*) override {}
  void CloseDialog(views::Widget::ClosedReason) override {}
  void ResizeDialog() override {}
};

}  // namespace

class DownloadBubbleSecurityViewTest : public ChromeViewsTestBase {
 public:
  DownloadBubbleSecurityViewTest()
      : manager_(std::make_unique<
                 testing::NiceMock<content::MockDownloadManager>>()),
        testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kNoFirstRun);
  }

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    profile_ = testing_profile_manager_.CreateTestingProfile("testing_profile");
    EXPECT_CALL(*manager_.get(), GetBrowserContext())
        .WillRepeatedly(testing::Return(profile_.get()));
    window_ = std::make_unique<TestBrowserWindow>();
    Browser::CreateParams params(profile_, true);
    params.type = Browser::TYPE_NORMAL;
    params.window = window_.get();
    browser_ = std::unique_ptr<Browser>(Browser::Create(params));

    anchor_widget_ = CreateTestWidget(views::Widget::InitParams::TYPE_WINDOW);
    auto bubble_delegate = std::make_unique<views::BubbleDialogDelegate>(
        anchor_widget_->GetContentsView(), views::BubbleBorder::TOP_RIGHT);
    bubble_delegate_ = bubble_delegate.get();
    bubble_navigator_ = std::make_unique<MockDownloadBubbleNavigationHandler>();
    security_view_ = bubble_delegate_->SetContentsView(
        std::make_unique<DownloadBubbleSecurityView>(bubble_controller_.get(),
                                                     bubble_navigator_.get(),
                                                     bubble_delegate_));
    views::BubbleDialogDelegate::CreateBubble(std::move(bubble_delegate));
    bubble_delegate_->GetWidget()->Show();
    bubble_controller_ =
        std::make_unique<MockDownloadBubbleUIController>(browser_.get());

    row_list_view_ = std::make_unique<DownloadBubbleRowListView>(
        /*is_partial_view=*/true, browser_.get());
    const int bubble_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
        views::DISTANCE_BUBBLE_PREFERRED_WIDTH);
    row_view_ = std::make_unique<DownloadBubbleRowView>(
        DownloadItemModel::Wrap(&download_item_), row_list_view_.get(),
        bubble_controller_.get(), bubble_navigator_.get(), browser_.get(),
        bubble_width);
  }

  void TearDown() override {
    // All windows need to be closed before tear down.
    anchor_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  DownloadBubbleSecurityViewTest(const DownloadBubbleSecurityViewTest&) =
      delete;
  DownloadBubbleSecurityViewTest& operator=(
      const DownloadBubbleSecurityViewTest&) = delete;

  raw_ptr<views::BubbleDialogDelegate> bubble_delegate_;
  std::unique_ptr<MockDownloadBubbleUIController> bubble_controller_;
  std::unique_ptr<MockDownloadBubbleNavigationHandler> bubble_navigator_;
  raw_ptr<DownloadBubbleSecurityView> security_view_;
  std::unique_ptr<views::Widget> anchor_widget_;

  testing::NiceMock<download::MockDownloadItem> download_item_;
  std::unique_ptr<DownloadBubbleRowListView> row_list_view_;
  std::unique_ptr<DownloadBubbleRowView> row_view_;

  std::unique_ptr<testing::NiceMock<content::MockDownloadManager>> manager_;
  TestingProfileManager testing_profile_manager_;
  raw_ptr<Profile> profile_;
  std::unique_ptr<TestBrowserWindow> window_;
  std::unique_ptr<Browser> browser_;
};

TEST_F(DownloadBubbleSecurityViewTest,
       UpdateSecurityView_WillHaveAppropriateDialogButtons) {
  // Two buttons, one prominent
  row_view_->SetUIInfoForTesting(
      DownloadUIModel::BubbleUIInfo(std::u16string())
          .AddIconAndColor(views::kInfoIcon, ui::kColorAlertHighSeverity)
          .AddPrimaryButton(DownloadCommands::Command::KEEP)
          // OK button
          .AddSubpageButton(std::u16string(),
                            DownloadCommands::Command::DISCARD,
                            /*is_prominent=*/true)
          // Cancel button
          .AddSubpageButton(std::u16string(), DownloadCommands::Command::KEEP,
                            /*is_prominent=*/false));
  security_view_->UpdateSecurityView(row_view_.get());
  EXPECT_EQ(bubble_delegate_->GetDialogButtons(),
            ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
  EXPECT_EQ(bubble_delegate_->GetDefaultDialogButton(), ui::DIALOG_BUTTON_OK);

  // Two buttons, none prominent
  row_view_->SetUIInfoForTesting(
      DownloadUIModel::BubbleUIInfo(std::u16string())
          .AddIconAndColor(views::kInfoIcon, ui::kColorAlertHighSeverity)
          .AddPrimaryButton(DownloadCommands::Command::KEEP)
          // OK button
          .AddSubpageButton(std::u16string(),
                            DownloadCommands::Command::DISCARD,
                            /*is_prominent=*/false)
          // Cancel button
          .AddSubpageButton(std::u16string(), DownloadCommands::Command::KEEP,
                            /*is_prominent=*/false));
  security_view_->UpdateSecurityView(row_view_.get());
  EXPECT_EQ(bubble_delegate_->GetDialogButtons(),
            ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
  EXPECT_EQ(bubble_delegate_->GetDefaultDialogButton(), ui::DIALOG_BUTTON_NONE);

  // One button, none prominent
  row_view_->SetUIInfoForTesting(
      DownloadUIModel::BubbleUIInfo(std::u16string())
          .AddIconAndColor(views::kInfoIcon, ui::kColorAlertHighSeverity)
          .AddPrimaryButton(DownloadCommands::Command::KEEP)
          // OK button
          .AddSubpageButton(std::u16string(),
                            DownloadCommands::Command::DISCARD,
                            /*is_prominent=*/false));
  security_view_->UpdateSecurityView(row_view_.get());
  EXPECT_EQ(bubble_delegate_->GetDialogButtons(), ui::DIALOG_BUTTON_OK);
  EXPECT_EQ(bubble_delegate_->GetDefaultDialogButton(), ui::DIALOG_BUTTON_NONE);

  // No buttons, none prominent
  row_view_->SetUIInfoForTesting(
      DownloadUIModel::BubbleUIInfo(std::u16string())
          .AddIconAndColor(views::kInfoIcon, ui::kColorAlertHighSeverity)
          .AddPrimaryButton(DownloadCommands::Command::KEEP));
  security_view_->UpdateSecurityView(row_view_.get());
  EXPECT_EQ(bubble_delegate_->GetDialogButtons(), ui::DIALOG_BUTTON_NONE);
  EXPECT_EQ(bubble_delegate_->GetDefaultDialogButton(), ui::DIALOG_BUTTON_NONE);
}

TEST_F(DownloadBubbleSecurityViewTest, VerifyLogWarningActions) {
  DownloadItemWarningData::AddWarningActionEvent(
      &download_item_, WarningSurface::BUBBLE_MAINPAGE, WarningAction::SHOWN);

  // Back action logged.
  {
    auto security_view = std::make_unique<DownloadBubbleSecurityView>(
        bubble_controller_.get(), bubble_navigator_.get(), bubble_delegate_);
    security_view->UpdateSecurityView(row_view_.get());

    security_view->BackButtonPressed();

    // Delete early to ensure DISMISS is not logged if BACK is already logged.
    security_view.reset();
    std::vector<WarningActionEvent> events =
        DownloadItemWarningData::GetWarningActionEvents(&download_item_);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].action, WarningAction::BACK);
  }

  // Close action logged
  {
    auto security_view = std::make_unique<DownloadBubbleSecurityView>(
        bubble_controller_.get(), bubble_navigator_.get(), bubble_delegate_);
    security_view->UpdateSecurityView(row_view_.get());

    security_view->CloseBubble();

    security_view.reset();
    std::vector<WarningActionEvent> events =
        DownloadItemWarningData::GetWarningActionEvents(&download_item_);
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[1].action, WarningAction::CLOSE);
  }

  // Dismiss action logged
  {
    auto security_view = std::make_unique<DownloadBubbleSecurityView>(
        bubble_controller_.get(), bubble_navigator_.get(), bubble_delegate_);
    security_view->UpdateSecurityView(row_view_.get());

    security_view.reset();
    std::vector<WarningActionEvent> events =
        DownloadItemWarningData::GetWarningActionEvents(&download_item_);
    ASSERT_EQ(events.size(), 3u);
    EXPECT_EQ(events[2].action, WarningAction::DISMISS);
  }

  // Dismiss action logged after update
  {
    auto security_view = std::make_unique<DownloadBubbleSecurityView>(
        bubble_controller_.get(), bubble_navigator_.get(), bubble_delegate_);
    security_view->UpdateSecurityView(row_view_.get());

    security_view->BackButtonPressed();

    // The security view can be re-opened after back button is pressed.
    security_view->UpdateSecurityView(row_view_.get());

    security_view.reset();
    std::vector<WarningActionEvent> events =
        DownloadItemWarningData::GetWarningActionEvents(&download_item_);
    ASSERT_EQ(events.size(), 5u);
    EXPECT_EQ(events[3].action, WarningAction::BACK);
    EXPECT_EQ(events[4].action, WarningAction::DISMISS);
  }
}
