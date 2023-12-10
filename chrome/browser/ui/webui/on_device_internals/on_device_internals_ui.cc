// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/on_device_internals/on_device_internals_ui.h"

#include <tuple>

#include "base/task/thread_pool.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/on_device_internals_resources.h"
#include "chrome/grit/on_device_internals_resources_map.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/on_device_model/public/cpp/model_assets.h"

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

WEB_UI_CONTROLLER_TYPE_IMPL(OnDeviceInternalsUI)

void OnDeviceInternalsUI::BindInterface(
    mojo::PendingReceiver<mojom::OnDeviceInternalsPage> receiver) {
  page_receivers_.Add(this, std::move(receiver));
}

void OnDeviceInternalsUI::LoadModel(
    const base::FilePath& model_path,
    mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
    LoadModelCallback callback) {
  // Warm the service while assets load in the background.
  std::ignore = GetService();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&on_device_model::LoadModelAssets, model_path, model_path),
      base::BindOnce(&OnDeviceInternalsUI::OnModelAssetsLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(model),
                     std::move(callback)));
}

on_device_model::mojom::OnDeviceModelService&
OnDeviceInternalsUI::GetService() {
  if (!service_) {
    content::ServiceProcessHost::Launch<
        on_device_model::mojom::OnDeviceModelService>(
        service_.BindNewPipeAndPassReceiver(),
        content::ServiceProcessHost::Options()
            .WithDisplayName("On-Device Model Service")
            .Pass());
  }
  return *service_.get();
}

void OnDeviceInternalsUI::GetEstimatedPerformanceClass(
    GetEstimatedPerformanceClassCallback callback) {
  GetService().GetEstimatedPerformanceClass(std::move(callback));
}

void OnDeviceInternalsUI::OnModelAssetsLoaded(
    mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
    LoadModelCallback callback,
    on_device_model::ModelAssets assets) {
  GetService().LoadModel(
      on_device_model::mojom::LoadModelParams::New(std::move(assets), 4096),
      std::move(model), std::move(callback));
}
