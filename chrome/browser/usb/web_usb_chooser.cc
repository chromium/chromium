// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/web_usb_chooser.h"

#include <utility>

#include "build/build_config.h"
#include "chrome/browser/usb/usb_chooser_controller.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/usb/android/web_usb_chooser_android.h"
#else
#include "chrome/browser/usb/web_usb_chooser_desktop.h"
#endif  // BUILDFLAG(IS_ANDROID)

// static
std::unique_ptr<WebUsbChooser> WebUsbChooser::Create(
    content::RenderFrameHost* render_frame_host,
    std::unique_ptr<UsbChooserController> controller) {
  std::unique_ptr<WebUsbChooser> chooser;
#if BUILDFLAG(IS_ANDROID)
  chooser = std::make_unique<WebUsbChooserAndroid>();
#else
  chooser = std::make_unique<WebUsbChooserDesktop>();
#endif  // BUILDFLAG(IS_ANDROID)
  chooser->ShowChooser(render_frame_host, std::move(controller));
  return chooser;
}

WebUsbChooser::~WebUsbChooser() = default;

WebUsbChooser::WebUsbChooser() = default;
