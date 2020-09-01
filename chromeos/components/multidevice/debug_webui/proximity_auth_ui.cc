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
#include "chromeos/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {

namespace multidevice {

ProximityAuthUI::ProximityAuthUI(
    content::WebUI* web_ui,
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client,
    MultiDeviceSetupBinder multidevice_setup_binder)
    : ui::MojoWebUIController(web_ui, true /* enable_chrome_send */),
      multidevice_setup_binder_(std::move(multidevice_setup_binder)) {
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

  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, source);
  web_ui->AddMessageHandler(std::make_unique<ProximityAuthWebUIHandler>(
      device_sync_client, secure_channel_client));
}

ProximityAuthUI::~ProximityAuthUI() = default;

void ProximityAuthUI::BindInterface(
    mojo::PendingReceiver<chromeos::multidevice_setup::mojom::MultiDeviceSetup>
        receiver) {
  multidevice_setup_binder_.Run(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(ProximityAuthUI)

}  // namespace multidevice

}  // namespace chromeos
