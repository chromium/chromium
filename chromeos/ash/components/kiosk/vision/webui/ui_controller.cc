// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/kiosk/vision/webui/ui_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "chromeos/ash/components/grit/kiosk_vision_internals_resources.h"
#include "chromeos/ash/components/grit/kiosk_vision_internals_resources_map.h"
#include "chromeos/ash/components/kiosk/vision/webui/constants.h"
#include "chromeos/ash/components/kiosk/vision/webui/kiosk_vision_internals.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash::kiosk_vision {

BASE_FEATURE(kEnableKioskVisionInternalsPage,
             "EnableKioskVisionInternalsPage",
             base::FEATURE_DISABLED_BY_DEFAULT);

UIController::UIController(content::WebUI* web_ui,
                           SetupWebUIDataSourceCallback setup_callback)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      std::string(kChromeUIKioskVisionInternalsHost));

  setup_callback.Run(source,
                     base::make_span(kKioskVisionInternalsResources,
                                     kKioskVisionInternalsResourcesSize),
                     IDR_KIOSK_VISION_INTERNALS_KIOSK_VISION_INTERNALS_HTML);
}

UIController::~UIController() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(UIController)

void UIController::BindInterface(
    mojo::PendingReceiver<mojom::PageConnector> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void UIController::BindPage(mojo::PendingRemote<mojom::Page> page_remote) {
  page_.reset();
  page_.Bind(std::move(page_remote));
}

UIConfig::UIConfig(SetupWebUIDataSourceCallback setup_callback)
    : WebUIConfig(content::kChromeUIScheme, kChromeUIKioskVisionInternalsHost),
      setup_callback_(std::move(setup_callback)) {}

UIConfig::~UIConfig() = default;

bool UIConfig::IsWebUIEnabled(content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(kEnableKioskVisionInternalsPage);
}

std::unique_ptr<content::WebUIController> UIConfig::CreateWebUIController(
    content::WebUI* web_ui,
    const GURL& url) {
  return std::make_unique<UIController>(web_ui, setup_callback_);
}

}  // namespace ash::kiosk_vision
