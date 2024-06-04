// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_USB_SYSTEM_TRAY_ICON_H_
#define CHROME_BROWSER_USB_USB_SYSTEM_TRAY_ICON_H_

#include <string>
#include "chrome/browser/device_notifications/device_system_tray_icon.h"
#include "ui/gfx/image/image_skia.h"

class UsbSystemTrayIcon : public DeviceSystemTrayIcon {
 public:
  explicit UsbSystemTrayIcon(
      std::unique_ptr<DeviceSystemTrayIconRenderer> icon_renderer);
  UsbSystemTrayIcon(const UsbSystemTrayIcon&) = delete;
  UsbSystemTrayIcon& operator=(const UsbSystemTrayIcon&) = delete;
  ~UsbSystemTrayIcon() override;

  const gfx::VectorIcon& GetIcon() override;
  std::u16string GetTitleLabel(size_t num_origins,
                               size_t num_connections) override;
  std::u16string GetContentSettingsLabel() override;

 private:
  DeviceConnectionTracker* GetConnectionTracker(
      base::WeakPtr<Profile> profile) override;
};

#endif  // CHROME_BROWSER_USB_USB_SYSTEM_TRAY_ICON_H_
