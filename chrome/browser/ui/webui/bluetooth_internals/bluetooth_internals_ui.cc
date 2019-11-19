// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals_ui.h"

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/bluetooth_internals_resources.h"
#include "chrome/grit/bluetooth_internals_resources_map.h"
#include "content/public/browser/web_ui_data_source.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/bluetooth/debug_logs_manager_factory.h"
#endif

BluetoothInternalsUI::BluetoothInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  // Set up the chrome://bluetooth-internals source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIBluetoothInternalsHost);

  // Add required resources.
  html_source->AddResourcePath("adapter.mojom-lite.js",
                               IDR_BLUETOOTH_INTERNALS_ADAPTER_MOJO_JS);
  html_source->AddResourcePath("device.mojom-lite.js",
                               IDR_BLUETOOTH_INTERNALS_DEVICE_MOJO_JS);
  html_source->AddResourcePath("bluetooth_internals.mojom-lite.js",
                               IDR_BLUETOOTH_INTERNALS_MOJO_JS);
  html_source->AddResourcePath("uuid.mojom-lite.js",
                               IDR_BLUETOOTH_INTERNALS_UUID_MOJO_JS);
  for (size_t i = 0; i < kBluetoothInternalsResourcesSize; i++) {
    html_source->AddResourcePath(kBluetoothInternalsResources[i].name,
                                 kBluetoothInternalsResources[i].value);
  }
  html_source->SetDefaultResource(IDR_BLUETOOTH_INTERNALS_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, html_source);
  AddHandlerToRegistry(
      base::BindRepeating(&BluetoothInternalsUI::BindBluetoothInternalsHandler,
                          base::Unretained(this)));
}

BluetoothInternalsUI::~BluetoothInternalsUI() {}

void BluetoothInternalsUI::BindBluetoothInternalsHandler(
    mojo::PendingReceiver<mojom::BluetoothInternalsHandler> receiver) {
  page_handler_ =
      std::make_unique<BluetoothInternalsHandler>(std::move(receiver));
#if defined(OS_CHROMEOS)
  page_handler_->set_debug_logs_manager(
      chromeos::bluetooth::DebugLogsManagerFactory::GetForProfile(
          Profile::FromWebUI(web_ui())));
#endif
}
