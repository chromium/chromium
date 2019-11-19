// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/usb_internals/usb_internals_ui.h"

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/usb_internals/usb_internals_page_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/usb_internals_resources.h"
#include "content/public/browser/web_ui_data_source.h"

UsbInternalsUI::UsbInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  // Set up the chrome://usb-internals source.
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIUsbInternalsHost);

  source->AddResourcePath("usb_internals.css", IDR_USB_INTERNALS_CSS);
  source->AddResourcePath("usb_internals.js", IDR_USB_INTERNALS_JS);
  source->AddResourcePath("usb_internals.mojom-lite.js",
                          IDR_USB_INTERNALS_MOJOM_LITE_JS);
  source->AddResourcePath("descriptor_panel.js",
                          IDR_USB_INTERNALS_DESCRIPTOR_PANEL_JS);
  source->AddResourcePath("devices_page.js", IDR_USB_INTERNALS_DEVICES_PAGE_JS);
  source->AddResourcePath("usb_device.mojom-lite.js",
                          IDR_USB_DEVICE_MOJOM_LITE_JS);
  source->AddResourcePath("usb_enumeration_options.mojom-lite.js",
                          IDR_USB_ENUMERATION_OPTIONS_MOJOM_LITE_JS);
  source->AddResourcePath("usb_manager.mojom-lite.js",
                          IDR_USB_DEVICE_MANAGER_MOJOM_LITE_JS);
  source->AddResourcePath("usb_manager_client.mojom-lite.js",
                          IDR_USB_DEVICE_MANAGER_CLIENT_MOJOM_LITE_JS);
  source->AddResourcePath("usb_manager_test.mojom-lite.js",
                          IDR_USB_DEVICE_MANAGER_TEST_MOJOM_LITE_JS);

  source->SetDefaultResource(IDR_USB_INTERNALS_HTML);

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);
  AddHandlerToRegistry(base::BindRepeating(
      &UsbInternalsUI::BindUsbInternalsPageHandler, base::Unretained(this)));
}

UsbInternalsUI::~UsbInternalsUI() {}

void UsbInternalsUI::BindUsbInternalsPageHandler(
    mojo::PendingReceiver<mojom::UsbInternalsPageHandler> receiver) {
  page_handler_ =
      std::make_unique<UsbInternalsPageHandler>(std::move(receiver));
}
