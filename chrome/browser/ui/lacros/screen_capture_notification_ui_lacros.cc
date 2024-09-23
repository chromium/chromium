// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lacros/screen_capture_notification_ui_lacros.h"

#include "chrome/browser/notifications/notification_platform_bridge_chromeos.h"
#include "chrome/browser/notifications/notification_platform_bridge_lacros.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/crosapi/mojom/message_center.mojom.h"
#include "chromeos/crosapi/mojom/notification.mojom-shared.h"
#include "chromeos/crosapi/mojom/notification.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

ScreenCaptureNotificationUILacros::ScreenCaptureNotificationUILacros(
    const std::u16string& text)
    : text_(text) {}

ScreenCaptureNotificationUILacros::~ScreenCaptureNotificationUILacros() =
    default;

gfx::NativeViewId ScreenCaptureNotificationUILacros::OnStarted(
    base::OnceClosure stop_callback,
    content::MediaStreamUI::SourceCallback source_callback,
    const std::vector<content::DesktopMediaID>& media_ids) {
  // Change source is not supported for window and screen sharing. Ignore
  // source_callback for simplicity.

  stop_callback_ = std::move(stop_callback);
  bridge_delegate_ = std::make_unique<NotificationPlatformBridgeChromeOs>();

  message_center::RichNotificationData data;
  data.buttons.emplace_back(
      l10n_util::GetStringUTF16(IDS_LACROS_STATUS_TRAY_SCREEN_ACCESS_STOP));

  auto delegate =
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              [](base::WeakPtr<ScreenCaptureNotificationUILacros> controller,
                 std::optional<int> button_index) {
                if (!button_index || !controller) {
                  return;
                }
                if (*button_index == 0) {
                  controller->ProcessStopRequestFromNotification();
                } else {
                  NOTREACHED_IN_MIGRATION();
                }
              },
              weak_ptr_factory_.GetWeakPtr()));

  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      kLacrosScreenAccessNotificationId,
      l10n_util::GetStringUTF16(IDS_LACROS_STATUS_TRAY_SCREEN_SHARE_TITLE),
      text_, ui::ImageModel(), std::u16string(), GURL(),
      message_center::NotifierId(), data, delegate);

  notification.set_pinned(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  bridge_delegate_->Display(NotificationHandler::Type::TRANSIENT, profile,
                            notification, nullptr);
  return 0;
}

void ScreenCaptureNotificationUILacros::ProcessStopRequestFromNotification() {
  if (!stop_callback_.is_null()) {
    std::move(stop_callback_).Run();
  }
}

// static
std::unique_ptr<ScreenCaptureNotificationUI>
ScreenCaptureNotificationUI::Create(
    const std::u16string& text,
    content::WebContents* capturing_web_contents) {
  return std::make_unique<ScreenCaptureNotificationUILacros>(text);
}
