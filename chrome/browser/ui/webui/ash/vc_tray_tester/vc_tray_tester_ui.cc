// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/ash/vc_tray_tester/vc_tray_tester_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/vc_tray_tester_resources.h"
#include "chrome/grit/vc_tray_tester_resources_map.h"
#include "content/public/browser/web_ui_data_source.h"

namespace ash {

VcTrayTesterUI::VcTrayTesterUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://vc-tray-tester source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(Profile::FromWebUI(web_ui),
                                             chrome::kChromeUIVcTrayTesterHost);

  // Add required resources.
  webui::SetupWebUIDataSource(
      html_source,
      base::make_span(kVcTrayTesterResources, kVcTrayTesterResourcesSize),
      IDR_VC_TRAY_TESTER_MAIN_HTML);

  // Add message handler.
  // web_ui->AddMessageHandler(std::make_unique<NotificationTesterHandler>());
}

VcTrayTesterUI::~VcTrayTesterUI() = default;

VcTrayTesterUIConfig::VcTrayTesterUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIVcTrayTesterHost) {}

}  // namespace ash
