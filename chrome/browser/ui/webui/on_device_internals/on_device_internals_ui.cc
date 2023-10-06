// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/on_device_internals/on_device_internals_ui.h"

#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/on_device_internals_resources.h"
#include "chrome/grit/on_device_internals_resources_map.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"

OnDeviceInternalsUI::OnDeviceInternalsUI(content::WebUI* web_ui)
    : MojoWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIOnDeviceInternalsHost);
  webui::SetupWebUIDataSource(source,
                              base::make_span(kOnDeviceInternalsResources,
                                              kOnDeviceInternalsResourcesSize),
                              IDR_ON_DEVICE_INTERNALS_ON_DEVICE_INTERNALS_HTML);
}

OnDeviceInternalsUI::~OnDeviceInternalsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(OnDeviceInternalsUI);

void OnDeviceInternalsUI::BindInterface(
    mojo::PendingReceiver<on_device_model::mojom::OnDeviceModelService>
        receiver) {
  content::ServiceProcessHost::Launch<
      on_device_model::mojom::OnDeviceModelService>(
      std::move(receiver), content::ServiceProcessHost::Options()
                               .WithDisplayName("On-Device Model Service")
                               .Pass());
}
