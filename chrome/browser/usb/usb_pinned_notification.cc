// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_pinned_notification.h"

#include "chrome/browser/device_notifications/device_pinned_notification_renderer.h"
#include "chrome/grit/generated_resources.h"

UsbPinnedNotification::UsbPinnedNotification()
    : UsbSystemTrayIcon(std::make_unique<DevicePinnedNotificationRenderer>(
          this,
          "chrome://device_indicator/usb/",
#if BUILDFLAG(IS_CHROMEOS_ASH)
          ash::NotificationCatalogName::kWebUsb,
#endif
          IDS_WEBUSB_SYSTEM_TRAY_ICON_EXTENSION_LIST)) {
}

UsbPinnedNotification::~UsbPinnedNotification() = default;
