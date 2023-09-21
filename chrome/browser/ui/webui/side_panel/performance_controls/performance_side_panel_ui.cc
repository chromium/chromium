// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/performance_controls/performance_side_panel_ui.h"

#include <string>
#include <utility>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/side_panel/performance_controls/performance_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/side_panel_performance_resources.h"
#include "chrome/grit/side_panel_performance_resources_map.h"
#include "chrome/grit/side_panel_shared_resources.h"
#include "chrome/grit/side_panel_shared_resources_map.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

PerformanceSidePanelUI::PerformanceSidePanelUI(content::WebUI* web_ui)
    : ui::MojoBubbleWebUIController(web_ui, true) {
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
}

PerformanceSidePanelUI::~PerformanceSidePanelUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(PerformanceSidePanelUI)

void PerformanceSidePanelUI::BindInterface(
    mojo::PendingReceiver<side_panel::mojom::PerformancePageHandlerFactory>
        receiver) {
  performance_page_factory_receiver_.reset();
  performance_page_factory_receiver_.Bind(std::move(receiver));
}

void PerformanceSidePanelUI::CreatePerformancePageHandler(
    mojo::PendingRemote<side_panel::mojom::PerformancePage> page,
    mojo::PendingReceiver<side_panel::mojom::PerformancePageHandler> receiver) {
  performance_page_handler_ = std::make_unique<PerformancePageHandler>(
      std::move(receiver), std::move(page), this);
}
