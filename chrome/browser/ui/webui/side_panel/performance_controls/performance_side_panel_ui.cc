// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/performance_controls/performance_side_panel_ui.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/side_panel/performance_controls/battery_saver_card_handler.h"
#include "chrome/browser/ui/webui/side_panel/performance_controls/memory_saver_card_handler.h"
#include "chrome/browser/ui/webui/side_panel/performance_controls/performance_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/side_panel_performance_resources.h"
#include "chrome/grit/side_panel_performance_resources_map.h"
#include "chrome/grit/side_panel_shared_resources.h"
#include "chrome/grit/side_panel_shared_resources_map.h"
#include "components/performance_manager/public/features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

PerformanceSidePanelUI::PerformanceSidePanelUI(content::WebUI* web_ui,
                                               const GURL& url)
    : TopChromeWebUIController(web_ui, true) {
  Profile* const profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIPerformanceSidePanelHost);
  static constexpr webui::LocalizedString kLocalizedStrings[] = {};
  for (const auto& str : kLocalizedStrings) {
    webui::AddLocalizedString(source, str.name, str.id);
  }

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kSidePanelPerformanceResources,
                      kSidePanelPerformanceResourcesSize),
      IDR_SIDE_PANEL_PERFORMANCE_PERFORMANCE_HTML);
  source->AddResourcePaths(base::make_span(kSidePanelSharedResources,
                                           kSidePanelSharedResourcesSize));

  source->AddBoolean("isPerformanceCPUInterventionEnabled", false);
  source->AddBoolean("isPerformanceMemoryInterventionEnabled", false);

  url::Component query(0, static_cast<int>(url.query_piece().length()));
  url::Component key, value;
  while (url::ExtractQueryKeyValue(url.query_piece(), &query, &key, &value)) {
    if (url.query_piece().substr(key.begin, key.len) == "notifications") {
      std::string_view value_str =
          url.query_piece().substr(value.begin, value.len);
      source->AddString("sidePanelNotifications", std::string{value_str});
    }
  }
}

PerformanceSidePanelUI::~PerformanceSidePanelUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(PerformanceSidePanelUI)

void PerformanceSidePanelUI::BindInterface(
    mojo::PendingReceiver<side_panel::mojom::PerformancePageHandlerFactory>
        receiver) {
  performance_page_factory_receiver_.reset();
  performance_page_factory_receiver_.Bind(std::move(receiver));
}

void PerformanceSidePanelUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
        pending_receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(pending_receiver));
}

void PerformanceSidePanelUI::CreatePerformancePageHandler(
    mojo::PendingRemote<side_panel::mojom::PerformancePage> page,
    mojo::PendingReceiver<side_panel::mojom::PerformancePageHandler> receiver) {
  performance_page_handler_ = std::make_unique<PerformancePageHandler>(
      std::move(receiver), std::move(page), this);
}

void PerformanceSidePanelUI::CreateBatterySaverCardHandler(
    mojo::PendingRemote<side_panel::mojom::BatterySaverCard> battery_saver_card,
    mojo::PendingReceiver<side_panel::mojom::BatterySaverCardHandler>
        battery_saver_receiver) {
  battery_saver_card_handler_ = std::make_unique<BatterySaverCardHandler>(
      std::move(battery_saver_receiver), std::move(battery_saver_card));
}

void PerformanceSidePanelUI::CreateMemorySaverCardHandler(
    mojo::PendingRemote<side_panel::mojom::MemorySaverCard> memory_saver_card,
    mojo::PendingReceiver<side_panel::mojom::MemorySaverCardHandler>
        memory_saver_receiver) {
  memory_saver_card_handler_ = std::make_unique<MemorySaverCardHandler>(
      std::move(memory_saver_receiver), std::move(memory_saver_card), this);
}
