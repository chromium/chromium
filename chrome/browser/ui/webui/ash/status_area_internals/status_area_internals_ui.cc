// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/status_area_internals/status_area_internals_ui.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/status_area_internals/status_area_internals_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/status_area_internals_resources.h"
#include "chrome/grit/status_area_internals_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace ash {

StatusAreaInternalsUI::StatusAreaInternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://status-area-internals source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          Profile::FromWebUI(web_ui), chrome::kChromeUIStatusAreaInternalsHost);

  // Add required resources.
  webui::SetupWebUIDataSource(
      html_source,
      base::make_span(kStatusAreaInternalsResources,
                      kStatusAreaInternalsResourcesSize),
      IDR_STATUS_AREA_INTERNALS_MAIN_HTML);

  web_ui->AddMessageHandler(std::make_unique<StatusAreaInternalsHandler>());
}

StatusAreaInternalsUI::~StatusAreaInternalsUI() = default;

StatusAreaInternalsUIConfig::StatusAreaInternalsUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIStatusAreaInternalsHost) {}

}  // namespace ash
