// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/app_service_internals/app_service_internals_ui.h"

#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/app_service_internals/app_service_internals_page_handler_impl.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/app_service_internals_resources.h"
#include "chrome/grit/app_service_internals_resources_map.h"
#include "content/public/browser/web_ui_data_source.h"

AppServiceInternalsUIConfig::AppServiceInternalsUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIAppServiceInternalsHost) {}

AppServiceInternalsUI::AppServiceInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui), profile_(Profile::FromWebUI(web_ui)) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile_, chrome::kChromeUIAppServiceInternalsHost);
  webui::SetupWebUIDataSource(
      source,
      base::make_span(kAppServiceInternalsResources,
                      kAppServiceInternalsResourcesSize),
      IDR_APP_SERVICE_INTERNALS_INDEX_HTML);
}

void AppServiceInternalsUI::BindInterface(
    mojo::PendingReceiver<
        mojom::app_service_internals::AppServiceInternalsPageHandler>
        receiver) {
  handler_ = std::make_unique<AppServiceInternalsPageHandlerImpl>(
      profile_, std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(AppServiceInternalsUI)

AppServiceInternalsUI::~AppServiceInternalsUI() = default;
