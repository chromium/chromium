// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/kiosk/vision/webui/ui_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "base/containers/span.h"
#include "chromeos/ash/components/grit/kiosk_vision_internals_resources.h"
#include "chromeos/ash/components/grit/kiosk_vision_internals_resources_map.h"
#include "chromeos/ash/components/kiosk/vision/webui/constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace ash::kiosk_vision {

UIController::UIController(content::WebUI* web_ui,
                           SetupWebUIDataSourceCallback setup_callback)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      std::string(kChromeUIKioskVisionInternalsHost));

  setup_callback.Run(source,
                     base::make_span(kKioskVisionInternalsResources,
                                     kKioskVisionInternalsResourcesSize),
                     IDR_KIOSK_VISION_INTERNALS_KIOSK_VISION_INTERNALS_HTML);

  source->AddString("message", "Placeholder Kiosk Vision internals page.");
}

UIController::~UIController() = default;

UIConfig::UIConfig(SetupWebUIDataSourceCallback setup_callback)
    : WebUIConfig(content::kChromeUIScheme, kChromeUIKioskVisionInternalsHost),
      setup_callback_(std::move(setup_callback)) {}

UIConfig::~UIConfig() = default;

std::unique_ptr<content::WebUIController> UIConfig::CreateWebUIController(
    content::WebUI* web_ui,
    const GURL& url) {
  return std::make_unique<UIController>(web_ui, setup_callback_);
}

}  // namespace ash::kiosk_vision
