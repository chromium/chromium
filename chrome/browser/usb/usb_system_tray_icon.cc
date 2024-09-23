// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_system_tray_icon.h"

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/device_notifications/device_system_tray_icon_renderer.h"
#include "chrome/browser/usb/usb_connection_tracker_factory.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"

// static
const gfx::VectorIcon& UsbSystemTrayIcon::GetIcon() {
  return kTabUsbConnectedIcon;
}

// static
std::u16string UsbSystemTrayIcon::GetTitleLabel(size_t num_origins,
                                                size_t num_connections) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return l10n_util::GetPluralStringFUTF16(IDS_WEBUSB_SYSTEM_TRAY_ICON_TITLE,
                                          static_cast<int>(num_connections));
#else
  NOTREACHED();
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

// static
std::u16string UsbSystemTrayIcon::GetContentSettingsLabel() {
  return l10n_util::GetStringUTF16(IDS_WEBUSB_SYSTEM_TRAY_ICON_USB_SETTINGS);
}

UsbSystemTrayIcon::UsbSystemTrayIcon(
    std::unique_ptr<DeviceSystemTrayIconRenderer> icon_renderer)
    : DeviceSystemTrayIcon(std::move(icon_renderer)) {}

UsbSystemTrayIcon::~UsbSystemTrayIcon() = default;

DeviceConnectionTracker* UsbSystemTrayIcon::GetConnectionTracker(
    base::WeakPtr<Profile> profile) {
  if (!profile) {
    return nullptr;
  }
  return UsbConnectionTrackerFactory::GetForProfile(profile.get(),
                                                    /*create=*/false);
}
