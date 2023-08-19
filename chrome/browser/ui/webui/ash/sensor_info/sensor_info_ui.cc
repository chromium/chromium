// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/sensor_info/sensor_info_ui.h"
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
    : content::WebUIController(web_ui) {
  // Set up the chrome://sensor-info source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUISensorInfoHost);
  // Add required resources.
  webui::SetupWebUIDataSource(
      source, base::make_span(kSensorInfoResources, kSensorInfoResourcesSize),
      IDR_SENSOR_INFO_SENSOR_INFO_HTML);
}

SensorInfoUI::~SensorInfoUI() = default;

}  // namespace ash
