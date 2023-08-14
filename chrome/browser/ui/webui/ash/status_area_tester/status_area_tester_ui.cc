// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/status_area_tester/status_area_tester_ui.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/status_area_tester/status_area_tester_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/status_area_tester_resources.h"
#include "chrome/grit/status_area_tester_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace ash {

StatusAreaTesterUI::StatusAreaTesterUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://status-area-tester source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          Profile::FromWebUI(web_ui), chrome::kChromeUIStatusAreaTesterHost);

  // Add required resources.
  webui::SetupWebUIDataSource(html_source,
                              base::make_span(kStatusAreaTesterResources,
                                              kStatusAreaTesterResourcesSize),
                              IDR_STATUS_AREA_TESTER_MAIN_HTML);

  web_ui->AddMessageHandler(std::make_unique<StatusAreaTesterHandler>());
}

StatusAreaTesterUI::~StatusAreaTesterUI() = default;

StatusAreaTesterUIConfig::StatusAreaTesterUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIStatusAreaTesterHost) {}

}  // namespace ash
