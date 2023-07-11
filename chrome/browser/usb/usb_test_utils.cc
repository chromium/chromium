// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_test_utils.h"

#include "chrome/browser/device_notifications/device_status_icon_renderer.h"
#include "chrome/browser/usb/usb_connection_tracker.h"
#include "chrome/browser/usb/usb_system_tray_icon.h"

TestUsbConnectionTracker::TestUsbConnectionTracker(Profile* profile)
    : UsbConnectionTracker(profile), mock_device_connection_tracker_(profile) {}

TestUsbConnectionTracker::~TestUsbConnectionTracker() = default;

void TestUsbConnectionTracker::ShowContentSettingsExceptions() {
  mock_device_connection_tracker_.ShowContentSettingsExceptions();
}
void TestUsbConnectionTracker::ShowSiteSettings(const url::Origin& origin) {
  mock_device_connection_tracker_.ShowSiteSettings(origin);
}

TestUsbSystemTrayIcon::TestUsbSystemTrayIcon() : UsbSystemTrayIcon(nullptr) {}

TestUsbSystemTrayIcon::~TestUsbSystemTrayIcon() = default;

void TestUsbSystemTrayIcon::StageProfile(Profile* profile) {
  mock_device_system_tray_icon_.StageProfile(profile);
}

void TestUsbSystemTrayIcon::UnstageProfile(Profile* profile, bool immediate) {
  mock_device_system_tray_icon_.UnstageProfile(profile, immediate);
}

void TestUsbSystemTrayIcon::ProfileAdded(Profile* profile) {
  mock_device_system_tray_icon_.ProfileAdded(profile);
}

void TestUsbSystemTrayIcon::ProfileRemoved(Profile* profile) {
  mock_device_system_tray_icon_.ProfileRemoved(profile);
}

void TestUsbSystemTrayIcon::NotifyConnectionCountUpdated(Profile* profile) {
  mock_device_system_tray_icon_.NotifyConnectionCountUpdated(profile);
}
