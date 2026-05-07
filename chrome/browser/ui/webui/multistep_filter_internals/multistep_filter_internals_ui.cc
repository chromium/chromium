// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/multistep_filter_internals/multistep_filter_internals_ui.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "chrome/browser/multistep_filter/core/multistep_filter_log_router_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/multistep_filter_internals/multistep_filter_internals_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/multistep_filter_internals_resources.h"
#include "chrome/grit/multistep_filter_internals_resources_map.h"
#include "components/multistep_filter/core/features.h"
#include "components/multistep_filter/core/logging/multistep_filter_log_router.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

namespace multistep_filter_internals {

bool MultistepFilterInternalsUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return !browser_context->IsOffTheRecord() &&
         base::FeatureList::IsEnabled(multistep_filter::kMultistepFilter);
}

MultistepFilterInternalsUI::MultistepFilterInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIMultistepFilterInternalsHost);

  webui::SetupWebUIDataSource(
      source, kMultistepFilterInternalsResources,
      IDR_MULTISTEP_FILTER_INTERNALS_MULTISTEPS_FILTER_INTERNALS_HTML);
}

MultistepFilterInternalsUI::~MultistepFilterInternalsUI() = default;

void MultistepFilterInternalsUI::BindInterface(
    mojo::PendingReceiver<mojom::PageHandlerFactory> receiver) {
  // Reset any existing connection before binding a new receiver, which can
  // happen when the WebUI page is reloaded.
  factory_receiver_.reset();
  factory_receiver_.Bind(std::move(receiver));
}

void MultistepFilterInternalsUI::CreatePageHandler(
    mojo::PendingRemote<mojom::Page> page,
    mojo::PendingReceiver<mojom::PageHandler> receiver) {
  multistep_filter::MultistepFilterLogRouter* log_router =
      multistep_filter::MultistepFilterLogRouterFactory::GetForProfile(
          Profile::FromWebUI(web_ui()));
  page_handler_ = std::make_unique<MultistepFilterInternalsPageHandler>(
      std::move(receiver), std::move(page), log_router);
}

WEB_UI_CONTROLLER_TYPE_IMPL(MultistepFilterInternalsUI)

}  // namespace multistep_filter_internals
