// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/web_usb_chooser_desktop.h"

#include <utility>

#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/usb/usb_chooser_controller.h"
#include "content/public/browser/web_contents.h"

WebUsbChooserDesktop::WebUsbChooserDesktop(
    content::RenderFrameHost* render_frame_host)
    : WebUsbChooser(render_frame_host) {}

WebUsbChooserDesktop::~WebUsbChooserDesktop() = default;

void WebUsbChooserDesktop::ShowChooser(
    std::unique_ptr<UsbChooserController> controller) {
  closure_runner_.RunAndReset();
  closure_runner_.ReplaceClosure(chrome::ShowDeviceChooserDialog(
      render_frame_host(), std::move(controller)));
}

base::WeakPtr<WebUsbChooser> WebUsbChooserDesktop::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}
