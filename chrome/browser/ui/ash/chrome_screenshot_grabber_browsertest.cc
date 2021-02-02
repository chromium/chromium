// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/dlp/mock_dlp_content_manager.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/chrome_screenshot_grabber.h"
#include "chrome/browser/ui/ash/chrome_screenshot_grabber_test_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_observer.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

using testing::_;
using testing::Return;

// Parameterized by TemporaryHoldingSpace feature state.
class ChromeScreenshotGrabberBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool>,
      public ChromeScreenshotGrabberTestObserver,
      public ui::ClipboardObserver {
 public:
  ChromeScreenshotGrabberBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(
        ash::features::kTemporaryHoldingSpace, GetParam());
  }
  ~ChromeScreenshotGrabberBrowserTest() override = default;

  void SetUpOnMainThread() override {
    display_service_ = std::make_unique<NotificationDisplayServiceTester>(
        browser()->profile());
    display_service_->SetNotificationAddedClosure(base::BindRepeating(
        &ChromeScreenshotGrabberBrowserTest::OnNotificationAdded,
        base::Unretained(this)));
    policy::DlpContentManager::SetDlpContentManagerForTesting(
        &mock_dlp_content_manager_);
  }

  void SetTestObserver(ChromeScreenshotGrabber* chrome_screenshot_grabber,
                       ChromeScreenshotGrabberTestObserver* observer) {
    chrome_screenshot_grabber->test_observer_ = observer;
  }

  // Overridden from ui::ScreenshotGrabberObserver
  void OnScreenshotCompleted(
      ui::ScreenshotResult screenshot_result,
      const base::FilePath& screenshot_path) override {
    screenshot_result_ = screenshot_result;
    screenshot_path_ = screenshot_path;
  }

  void OnNotificationAdded() {
    notification_added_ = true;
    message_loop_runner_->Quit();
  }

  // Overridden from ui::ClipboardObserver
  void OnClipboardDataChanged() override {
    clipboard_changed_ = true;
    message_loop_runner_->Quit();
  }

  void RunLoop() {
    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
  }

  bool IsImageClipboardAvailable() {
    return ui::Clipboard::GetForCurrentThread()->IsFormatAvailable(
        ui::ClipboardFormatType::GetBitmapType(),
        ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr);
  }

  bool TemporaryHoldingSpaceEnabled() const { return GetParam(); }

  ash::HoldingSpaceModel* GetHoldingSpaceModel() const {
    ash::HoldingSpaceController* const controller =
        ash::HoldingSpaceController::Get();
    return controller ? controller->model() : nullptr;
  }

  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
  ui::ScreenshotResult screenshot_result_;
  base::FilePath screenshot_path_;
  bool notification_added_ = false;
  bool clipboard_changed_ = false;
  policy::MockDlpContentManager mock_dlp_content_manager_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ChromeScreenshotGrabberBrowserTest);
};

INSTANTIATE_TEST_SUITE_P(All,
                         ChromeScreenshotGrabberBrowserTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(ChromeScreenshotGrabberBrowserTest, TakeScreenshot) {
  ChromeScreenshotGrabber* chrome_screenshot_grabber =
      ChromeScreenshotGrabber::Get();
  SetTestObserver(chrome_screenshot_grabber, this);
  EXPECT_TRUE(chrome_screenshot_grabber->CanTakeScreenshot());

  chrome_screenshot_grabber->HandleTakeWindowScreenshot(
      ash::Shell::GetPrimaryRootWindow());

  EXPECT_FALSE(
      chrome_screenshot_grabber->screenshot_grabber()->CanTakeScreenshot());

  RunLoop();
  SetTestObserver(chrome_screenshot_grabber, nullptr);

  EXPECT_TRUE(notification_added_);
  auto notification =
      display_service_->GetNotification(std::string("screenshot"));
  ASSERT_TRUE(notification.has_value());
  EXPECT_EQ(message_center::NotifierType::SYSTEM_COMPONENT,
            notification->notifier_id().type);
  EXPECT_EQ("ash.screenshot", notification->notifier_id().id);
  EXPECT_EQ(GURL("chrome://screenshot"), notification->origin_url());
  EXPECT_EQ(message_center::SystemNotificationWarningLevel::NORMAL,
            notification->system_notification_warning_level());

  EXPECT_EQ(ui::ScreenshotResult::SUCCESS, screenshot_result_);
  {
    base::ScopedAllowBlockingForTesting allow_io;
    EXPECT_TRUE(base::PathExists(screenshot_path_));
  }

  if (TemporaryHoldingSpaceEnabled()) {
    ash::HoldingSpaceModel* holding_space_model = GetHoldingSpaceModel();
    ASSERT_TRUE(holding_space_model);
    ASSERT_EQ(1u, holding_space_model->items().size());

    ash::HoldingSpaceItem* holding_space_item =
        holding_space_model->items()[0].get();
    EXPECT_EQ(ash::HoldingSpaceItem::Type::kScreenshot,
              holding_space_item->type());
    EXPECT_EQ(screenshot_path_, holding_space_item->file_path());
  } else {
    EXPECT_FALSE(GetHoldingSpaceModel());
  }

  EXPECT_FALSE(IsImageClipboardAvailable());
  ui::ClipboardMonitor::GetInstance()->AddObserver(this);

  // Copy to clipboard button.
  display_service_->SimulateClick(NotificationHandler::Type::TRANSIENT,
                                  std::string("screenshot"), 0, base::nullopt);

  RunLoop();
  ui::ClipboardMonitor::GetInstance()->RemoveObserver(this);

  EXPECT_TRUE(clipboard_changed_);
  EXPECT_TRUE(IsImageClipboardAvailable());
}

IN_PROC_BROWSER_TEST_P(ChromeScreenshotGrabberBrowserTest,
                       ScreenshotsDisallowed) {
  ChromeScreenshotGrabber* chrome_screenshot_grabber =
      ChromeScreenshotGrabber::Get();
  chrome_screenshot_grabber->set_screenshots_allowed(false);
  SetTestObserver(chrome_screenshot_grabber, this);

  chrome_screenshot_grabber->HandleTakeWindowScreenshot(
      ash::Shell::GetPrimaryRootWindow());
  RunLoop();

  EXPECT_TRUE(notification_added_);
  auto notification =
      display_service_->GetNotification(std::string("screenshot"));
  EXPECT_TRUE(notification.has_value());
  EXPECT_EQ(message_center::SystemNotificationWarningLevel::CRITICAL_WARNING,
            notification->system_notification_warning_level());
  EXPECT_EQ(ui::ScreenshotResult::DISABLED, screenshot_result_);

  if (TemporaryHoldingSpaceEnabled()) {
    ash::HoldingSpaceModel* holding_space_model = GetHoldingSpaceModel();
    ASSERT_TRUE(holding_space_model);
    EXPECT_TRUE(holding_space_model->items().empty());
  } else {
    EXPECT_FALSE(GetHoldingSpaceModel());
  }
}

IN_PROC_BROWSER_TEST_P(ChromeScreenshotGrabberBrowserTest,
                       ScreenshotsRestricted) {
  ChromeScreenshotGrabber* chrome_screenshot_grabber =
      ChromeScreenshotGrabber::Get();
  SetTestObserver(chrome_screenshot_grabber, this);

  auto* window = ash::Shell::GetPrimaryRootWindow();

  EXPECT_CALL(mock_dlp_content_manager_, IsScreenshotRestricted(_))
      .Times(1)
      .WillOnce(Return(true));

  chrome_screenshot_grabber->HandleTakeWindowScreenshot(window);
  RunLoop();

  EXPECT_TRUE(notification_added_);
  auto notification =
      display_service_->GetNotification(std::string("screenshot"));
  EXPECT_TRUE(notification.has_value());
  EXPECT_EQ(message_center::SystemNotificationWarningLevel::CRITICAL_WARNING,
            notification->system_notification_warning_level());
  EXPECT_EQ(ui::ScreenshotResult::DISABLED_BY_DLP, screenshot_result_);

  if (TemporaryHoldingSpaceEnabled()) {
    ash::HoldingSpaceModel* holding_space_model = GetHoldingSpaceModel();
    ASSERT_TRUE(holding_space_model);
    EXPECT_TRUE(holding_space_model->items().empty());
  } else {
    EXPECT_FALSE(GetHoldingSpaceModel());
  }
}
