// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/ash/sensor_info/sensor_info_ui.h"

#include <memory>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/sensor_info_resources.h"
#include "chrome/grit/sensor_info_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace ash {
SensorInfoUI::SensorInfoUI(content::WebUI* web_ui)
    : MojoWebUIController(web_ui),
      profile_(Profile::FromWebUI(web_ui)),
      provider_(ash::SensorProvider()) {
  // Sets up the chrome://sensor-info source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUISensorInfoHost);
  // Adds required resources.
  webui::SetupWebUIDataSource(
      source, base::make_span(kSensorInfoResources, kSensorInfoResourcesSize),
      IDR_SENSOR_INFO_SENSOR_INFO_HTML);
}

SensorInfoUI::~SensorInfoUI() = default;

void SensorInfoUI::BindInterface(
    mojo::PendingReceiver<sensor::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void SensorInfoUI::CreatePageHandler(
    mojo::PendingReceiver<sensor::mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<SensorPageHandler>(profile_, &provider_,
                                                      std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(SensorInfoUI)
}  // namespace ash
