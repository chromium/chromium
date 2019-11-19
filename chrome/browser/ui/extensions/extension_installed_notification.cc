// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_installed_notification.h"

#include "ash/public/cpp/notification_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/apps/launch_service/launch_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/message_center/public/cpp/notification.h"

namespace {
const char kNotifierId[] = "app.downloaded-notification";
}  // namespace

using content::BrowserThread;

// static
void ExtensionInstalledNotification::Show(
    const extensions::Extension* extension, Profile* profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // It's lifetime is managed by the parent class NotificationDelegate.
  new ExtensionInstalledNotification(extension, profile);
}

ExtensionInstalledNotification::ExtensionInstalledNotification(
    const extensions::Extension* extension, Profile* profile)
    : extension_id_(extension->id()), profile_(profile) {
  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, extension_id_,
          base::UTF8ToUTF16(extension->name()),
          l10n_util::GetStringUTF16(IDS_EXTENSION_NOTIFICATION_INSTALLED),
          l10n_util::GetStringUTF16(IDS_EXTENSION_NOTIFICATION_DISPLAY_SOURCE),
          GURL(extension_urls::kChromeWebstoreBaseURL) /* origin_url */,
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT, kNotifierId),
          {}, this, kNotificationInstalledIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);

  NotificationDisplayService::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, *notification,
      /*metadata=*/nullptr);
}

ExtensionInstalledNotification::~ExtensionInstalledNotification() {}

void ExtensionInstalledNotification::Click(
    const base::Optional<int>& button_index,
    const base::Optional<base::string16>& reply) {
  if (!extensions::util::IsAppLaunchable(extension_id_, profile_))
    return;

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetExtensionById(
          extension_id_, extensions::ExtensionRegistry::EVERYTHING);
  if (!extension)
    return;

  apps::AppLaunchParams params = CreateAppLaunchParamsUserContainer(
      profile_, extension, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      apps::mojom::AppLaunchSource::kSourceInstalledNotification);
  apps::LaunchService::Get(profile_)->OpenApplication(params);
}
