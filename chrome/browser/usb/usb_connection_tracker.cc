// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_connection_tracker.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/usb/usb_system_tray_icon.h"

UsbConnectionTracker::UsbConnectionTracker(Profile* profile)
    : DeviceConnectionTracker(profile) {}

UsbConnectionTracker::~UsbConnectionTracker() = default;

void UsbConnectionTracker::ShowContentSettingsExceptions() {
  chrome::ShowContentSettingsExceptionsForProfile(
      profile_, ContentSettingsType::USB_CHOOSER_DATA);
}

void UsbConnectionTracker::Shutdown() {
  CleanUp();
  DeviceConnectionTracker::Shutdown();
}

DeviceSystemTrayIcon* UsbConnectionTracker::GetSystemTrayIcon() {
  return static_cast<DeviceSystemTrayIcon*>(
      g_browser_process->usb_system_tray_icon());
}
