// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/usb_internals/usb_internals_ui.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/usb_internals/usb_internals_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/usb_internals_resources.h"
#include "chrome/grit/usb_internals_resources_map.h"
#include "content/public/browser/web_ui_data_source.h"

UsbInternalsUI::UsbInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  // Set up the chrome://usb-internals source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIUsbInternalsHost);

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kUsbInternalsResources, kUsbInternalsResourcesSize),
      IDR_USB_INTERNALS_USB_INTERNALS_HTML);
}

WEB_UI_CONTROLLER_TYPE_IMPL(UsbInternalsUI)

UsbInternalsUI::~UsbInternalsUI() {}

void UsbInternalsUI::BindInterface(
    mojo::PendingReceiver<mojom::UsbInternalsPageHandler> receiver) {
  page_handler_ =
      std::make_unique<UsbInternalsPageHandler>(std::move(receiver));
}
