// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/web_usb_chooser_desktop.h"

#include <utility>

#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/usb/usb_chooser_controller.h"

WebUsbChooserDesktop::WebUsbChooserDesktop() = default;

WebUsbChooserDesktop::~WebUsbChooserDesktop() = default;

void WebUsbChooserDesktop::ShowChooser(
    content::RenderFrameHost* render_frame_host,
    std::unique_ptr<UsbChooserController> controller) {
  closure_runner_.ReplaceClosure(chrome::ShowDeviceChooserDialog(
      render_frame_host, std::move(controller)));
}
