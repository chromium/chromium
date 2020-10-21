// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/usb_internals/usb_internals_ui.h"

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/usb_internals/usb_internals_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/usb_internals_resources.h"
#include "content/public/browser/web_ui_data_source.h"

UsbInternalsUI::UsbInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  // Set up the chrome://usb-internals source.
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIUsbInternalsHost);

  static constexpr webui::ResourcePath kPaths[] = {
      {"app.js", IDR_USB_INTERNALS_APP_JS},
      {"usb_internals.css", IDR_USB_INTERNALS_CSS},
      {"usb_internals.mojom-lite.js", IDR_USB_INTERNALS_MOJOM_LITE_JS},
      {"descriptor_panel.js", IDR_USB_INTERNALS_DESCRIPTOR_PANEL_JS},
      {"devices_page.js", IDR_USB_INTERNALS_DEVICES_PAGE_JS},
      {"usb_device.mojom-lite.js", IDR_USB_DEVICE_MOJOM_LITE_JS},
      {"usb_enumeration_options.mojom-lite.js",
       IDR_USB_ENUMERATION_OPTIONS_MOJOM_LITE_JS},
      {"usb_manager.mojom-lite.js", IDR_USB_DEVICE_MANAGER_MOJOM_LITE_JS},
      {"usb_manager_client.mojom-lite.js",
       IDR_USB_DEVICE_MANAGER_CLIENT_MOJOM_LITE_JS},
      {"usb_manager_test.mojom-lite.js",
       IDR_USB_DEVICE_MANAGER_TEST_MOJOM_LITE_JS},
  };
  webui::AddResourcePathsBulk(source, kPaths);

  source->SetDefaultResource(IDR_USB_INTERNALS_HTML);
  source->DisableTrustedTypesCSP();

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);
}

WEB_UI_CONTROLLER_TYPE_IMPL(UsbInternalsUI)

UsbInternalsUI::~UsbInternalsUI() {}

void UsbInternalsUI::BindInterface(
    mojo::PendingReceiver<mojom::UsbInternalsPageHandler> receiver) {
  page_handler_ =
      std::make_unique<UsbInternalsPageHandler>(std::move(receiver));
}
