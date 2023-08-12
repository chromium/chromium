// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lacros/screen_capture_notification_ui_lacros.h"

#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/lacros/lacros_test_helper.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/gtest/include/gtest/gtest.h"

class ScreenCaptureNotificationUILacrosTest : public InProcessBrowserTest {
 public:
  ScreenCaptureNotificationUILacrosTest() = default;
  ~ScreenCaptureNotificationUILacrosTest() override = default;

  base::OnceCallback<void()> stop_callback_ =
      base::BindLambdaForTesting([this] { stop_callback_called_ = true; });

  bool stop_callback_called_ = false;
};

IN_PROC_BROWSER_TEST_F(ScreenCaptureNotificationUILacrosTest, OnStarted) {
  std::unique_ptr<ScreenCaptureNotificationUI> screen_capture_notification_ui =
      ScreenCaptureNotificationUI::Create(std::u16string(u"UI Test"));

  // OnStarted executed without issue.
  EXPECT_EQ(
      screen_capture_notification_ui->OnStarted(
          std::move(stop_callback_), content::MediaStreamUI::SourceCallback(),
          std::vector<content::DesktopMediaID>{}),
      0);
  auto& remote = chromeos::LacrosService::Get()
                     ->GetRemote<crosapi::mojom::MessageCenter>();
  ASSERT_TRUE(remote.get());

  // Get notification ids in system tray.
  base::test::TestFuture<const std::vector<std::string>&> ids_future;
  remote->GetDisplayedNotifications(ids_future.GetCallback());
  std::vector<std::string> notification_ids = ids_future.Get();

  std::string screen_capture_notification_profile_id =
      ProfileNotification::GetProfileNotificationId(
          kLacrosScreenAccessNotificationId,
          ProfileNotification::GetProfileID(browser()->profile()));

  bool screen_capture_notification_shown = false;
  for (int i = 0; i < static_cast<int>(notification_ids.size()); i++) {
    if (notification_ids[i] == screen_capture_notification_profile_id) {
      screen_capture_notification_shown = true;
    }
  }

  // Screen capture notification is indeed shown in system tray.
  EXPECT_TRUE(screen_capture_notification_shown);

  // Pressing on notification's "Stop" button invokes stop_callback_.
  EXPECT_FALSE(stop_callback_called_);
  ScreenCaptureNotificationUILacros* test =
      static_cast<ScreenCaptureNotificationUILacros*>(
          screen_capture_notification_ui.get());
  test->bridge_delegate_->HandleNotificationButtonClicked(
      screen_capture_notification_profile_id, 0, absl::nullopt);
  EXPECT_TRUE(stop_callback_called_);
}
