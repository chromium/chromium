// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/on_device_internals/on_device_internals_ui.h"

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/on_device_internals/on_device_internals_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/on_device_internals_resources.h"
#include "chrome/grit/on_device_internals_resources_map.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

namespace on_device_internals {

bool OnDeviceInternalsUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(
      optimization_guide::features::kOptimizationGuideOnDeviceModel);
}

OnDeviceInternalsUI::OnDeviceInternalsUI(content::WebUI* web_ui)
    : MojoWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIOnDeviceInternalsHost);
  webui::SetupWebUIDataSource(source, kOnDeviceInternalsResources,
                              IDR_ON_DEVICE_INTERNALS_ON_DEVICE_INTERNALS_HTML);
}

OnDeviceInternalsUI::~OnDeviceInternalsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(OnDeviceInternalsUI)

void OnDeviceInternalsUI::BindInterface(
    mojo::PendingReceiver<mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void OnDeviceInternalsUI::CreatePageHandler(
    mojo::PendingRemote<mojom::Page> page,
    mojo::PendingReceiver<mojom::PageHandler> receiver) {
  CHECK(page);

  Profile* profile = Profile::FromWebUI(web_ui());
  auto* service = OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (!service) {
    return;
  }
  page_handler_ = std::make_unique<PageHandler>(std::move(receiver),
                                                std::move(page), service);
}

}  // namespace on_device_internals
