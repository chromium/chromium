// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/reload_button/reload_button_ui.h"

#include <memory>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_widget.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter_service.h"
#include "chrome/browser/ui/webui/reload_button/reload_button.mojom.h"
#include "chrome/browser/ui/webui/reload_button/reload_button_page_handler.h"
#include "chrome/browser/ui/webui/theme_colors_source_manager.h"
#include "chrome/browser/ui/webui/theme_colors_source_manager_factory.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/reload_button_resources.h"
#include "chrome/grit/reload_button_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/views/widget/widget.h"
#include "ui/webui/webui_util.h"

ReloadButtonUI::ReloadButtonUI(content::WebUI* web_ui)
    // Sets `enable_chrome_send` to true to allow chrome.send() to be called in
    // TypeScript to record non-timestamp histograms, which can't be done by
    // MetricsReporter.
    : TopChromeWebUIController(web_ui, /*enable_chrome_send=*/true) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIReloadButtonHost);

  static constexpr webui::LocalizedString kStrings[] = {
      // go/keep-sorted start
      {"reloadButtonAccNameReload", IDS_ACCNAME_RELOAD},
      {"reloadButtonTooltipReload", IDS_TOOLTIP_RELOAD},
      {"reloadButtonTooltipReloadWithMenu", IDS_TOOLTIP_RELOAD_WITH_MENU},
      {"reloadButtonTooltipStop", IDS_TOOLTIP_STOP},
      // go/keep-sorted end
  };
  source->AddLocalizedStrings(kStrings);

  webui::SetupWebUIDataSource(source, kReloadButtonResources,
                              IDR_RELOAD_BUTTON_RELOAD_BUTTON_HTML);

  // Handles chrome.send() calls that records non-timestamp histograms.
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());
}

WEB_UI_CONTROLLER_TYPE_IMPL(ReloadButtonUI)

ReloadButtonUI::~ReloadButtonUI() = default;

ReloadButtonUIConfig::ReloadButtonUIConfig()
    : DefaultTopChromeWebUIConfig(content::kChromeUIScheme,
                                  chrome::kChromeUIReloadButtonHost) {}

bool ReloadButtonUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return features::IsWebUIReloadButtonEnabled();
}

void ReloadButtonUI::BindInterface(
    mojo::PendingReceiver<reload_button::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void ReloadButtonUI::SetReloadButtonState(bool is_loading,
                                          bool is_menu_enabled) {
  if (page_handler_) {
    page_handler_->SetReloadButtonState(is_loading, is_menu_enabled);
  }
}

void ReloadButtonUI::CreatePageHandler(
    mojo::PendingRemote<reload_button::mojom::Page> page,
    mojo::PendingReceiver<reload_button::mojom::PageHandler> receiver) {
  CHECK(page);
  auto* web_contents = web_ui()->GetWebContents();
  auto* command_updater = GetCommandUpdater();
  if (!command_updater) {
    return;
  }
  page_handler_ = std::make_unique<ReloadButtonPageHandler>(
      std::move(receiver), std::move(page), web_contents, command_updater);
}

ReloadButtonPageHandler* ReloadButtonUI::page_handler_for_testing() {
  return page_handler_.get();
}

CommandUpdater* ReloadButtonUI::GetCommandUpdater() const {
  if (command_updater_for_testing_) {
    return command_updater_for_testing_;  // IN-TEST
  }

  BrowserWindowInterface* browser_interface =
      webui::GetBrowserWindowInterface(web_ui()->GetWebContents());
  if (!browser_interface) {
    return nullptr;
  }
  return browser_interface->GetFeatures().browser_command_controller();
}

void ReloadButtonUI::SetCommandUpdaterForTesting(
    CommandUpdater* command_updater) {
  command_updater_for_testing_ = command_updater;
}

void ReloadButtonUI::PopulateLocalResourceLoaderConfig(
    blink::mojom::LocalResourceLoaderConfig* config,
    const url::Origin& requesting_origin) {
  auto* theme_colors_manager = ThemeColorsSourceManagerFactory::GetForProfile(
      Profile::FromWebUI(web_ui()));
  CHECK(theme_colors_manager);
  theme_colors_manager->PopulateLocalResourceLoaderConfig(
      config, requesting_origin, web_ui()->GetWebContents());
}
