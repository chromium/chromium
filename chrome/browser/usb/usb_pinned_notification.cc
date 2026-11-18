// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_pinned_notification.h"

#include "ash/strings/grit/ash_strings.h"
#include "chrome/browser/device_notifications/device_pinned_notification_renderer.h"

UsbPinnedNotification::UsbPinnedNotification()
    : UsbSystemTrayIcon(std::make_unique<DevicePinnedNotificationRenderer>(
          this,
          "chrome://device_indicator/usb/",
          ash::NotificationCatalogName::kWebUsb,
          IDS_WEBUSB_SYSTEM_TRAY_ICON_EXTENSION_LIST)) {
}

UsbPinnedNotification::~UsbPinnedNotification() = default;
