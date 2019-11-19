// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/multidevice/debug_webui/proximity_auth_ui.h"

#include <memory>

#include "base/bind.h"
#include "chromeos/components/multidevice/debug_webui/proximity_auth_webui_handler.h"
#include "chromeos/components/multidevice/debug_webui/url_constants.h"
#include "chromeos/grit/chromeos_resources.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/services/multidevice_setup/public/mojom/constants.mojom.h"
#include "chromeos/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {

namespace multidevice {

ProximityAuthUI::ProximityAuthUI(
    content::WebUI* web_ui,
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client)
    : ui::MojoWebUIController(web_ui, true /* enable_chrome_send */) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(kChromeUIProximityAuthHost);
  source->SetDefaultResource(IDR_MULTIDEVICE_INDEX_HTML);
  source->AddResourcePath("common.css", IDR_MULTIDEVICE_COMMON_CSS);
  source->AddResourcePath("webui.js", IDR_MULTIDEVICE_WEBUI_JS);
  source->AddResourcePath("logs.js", IDR_MULTIDEVICE_LOGS_JS);
  source->AddResourcePath("proximity_auth.html",
                          IDR_MULTIDEVICE_PROXIMITY_AUTH_HTML);
  source->AddResourcePath("proximity_auth.css",
                          IDR_MULTIDEVICE_PROXIMITY_AUTH_CSS);
  source->AddResourcePath("proximity_auth.js",
                          IDR_MULTIDEVICE_PROXIMITY_AUTH_JS);
  source->AddResourcePath("pollux.html", IDR_MULTIDEVICE_POLLUX_HTML);
  source->AddResourcePath("pollux.css", IDR_MULTIDEVICE_POLLUX_CSS);
  source->AddResourcePath("pollux.js", IDR_MULTIDEVICE_POLLUX_JS);

  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, source);
  web_ui->AddMessageHandler(std::make_unique<ProximityAuthWebUIHandler>(
      device_sync_client, secure_channel_client));
  AddHandlerToRegistry(base::BindRepeating(
      &ProximityAuthUI::BindMultiDeviceSetup, base::Unretained(this)));
}

ProximityAuthUI::~ProximityAuthUI() = default;

void ProximityAuthUI::BindMultiDeviceSetup(
    mojo::PendingReceiver<chromeos::multidevice_setup::mojom::MultiDeviceSetup>
        receiver) {
  service_manager::Connector* connector =
      content::BrowserContext::GetConnectorFor(
          web_ui()->GetWebContents()->GetBrowserContext());
  DCHECK(connector);

  connector->Connect(chromeos::multidevice_setup::mojom::kServiceName,
                     std::move(receiver));
}

}  // namespace multidevice

}  // namespace chromeos
