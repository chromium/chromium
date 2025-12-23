// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/reload_button/reload_button_ui.h"

#include <memory>
#include <utility>

#include "base/check_is_test.h"
#include "base/debug/dump_without_crashing.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/color_provider_browser_helper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_widget.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter_service.h"
#include "chrome/browser/ui/webui/reload_button/reload_button.mojom.h"
#include "chrome/browser/ui/webui/reload_button/reload_button_page_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/reload_button_resources.h"
#include "chrome/grit/reload_button_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "third_party/blink/public/mojom/loader/local_resource_loader_config.mojom.h"
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
      {"reloadButtonTooltipReloadWithMenu", IDS_TOOLTIP_RELOAD_WITH_MENU},
      {"reloadButtonTooltipReload", IDS_TOOLTIP_RELOAD},
      {"reloadButtonTooltipStop", IDS_TOOLTIP_STOP}};
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

  return webui::GetBrowserWindowInterface(web_ui()->GetWebContents())
      ->GetFeatures()
      .browser_command_controller();
}

void ReloadButtonUI::SetCommandUpdaterForTesting(
    CommandUpdater* command_updater) {
  command_updater_for_testing_ = command_updater;
}

void ReloadButtonUI::SetColorProviderForTesting(
    const ui::ColorProvider* color_provider) {
  CHECK_IS_TEST();
  color_provider_for_testing_ = color_provider;
}

void ReloadButtonUI::PopulateLocalResourceLoaderConfig(
    blink::mojom::LocalResourceLoaderConfig* config,
    const url::Origin& requesting_origin) {
  // TODO(crbug.com/457618790): Refactor the following into a profile service
  // and ensure we have ColorProviders available early in View init separately.
  const ui::ColorProvider* color_provider = nullptr;
  if (color_provider_for_testing_) {
    CHECK_IS_TEST();
    color_provider = color_provider_for_testing_.get();
  } else {
    auto* browser_window = BrowserWindow::FindBrowserWindowWithWebContents(
        web_ui()->GetWebContents());
    if (browser_window) {
      color_provider = browser_window->GetColorProvider();
    }
    // Fallback to ThemeService if we couldn't get the ColorProvider from the
    // BrowserWindow. This might happen if the platform doesn't fully initialize
    // the native views yet.
    if (!color_provider) {
      auto* theme_service =
          ThemeServiceFactory::GetForProfile(Profile::FromWebUI(web_ui()));
      if (theme_service) {
        color_provider = theme_service->GetColorProvider();
        // Should not happen in normal operation.
        base::debug::DumpWithoutCrashing();
      }
    }
  }
  CHECK(color_provider);

  // Generate the colors CSS.
  // We use the Widget's ColorProvider to ensure we match the window's theme.
  GURL colors_css_url(ThemeSource::kThemeColorsCssUrl);
  const auto* theme_service = ThemeServiceFactory::GetForProfile(
      Profile::FromWebUI(web_ui())->GetOriginalProfile());
  std::optional<std::string> css_content = ThemeSource::GenerateColorsCss(
      *color_provider, colors_css_url, theme_service->GetIsGrayscale(),
      theme_service->GetIsBaseline());
  if (!css_content) {
    return;
  }

  auto source = blink::mojom::LocalResourceSource::New();
  if (requesting_origin.scheme() == content::kChromeUIScheme) {
    source->headers =
        "Access-Control-Allow-Origin: " + requesting_origin.Serialize();
  }
  source->path_to_resource_map[colors_css_url.path().substr(1)] =
      blink::mojom::LocalResourceValue::NewResponseBody(
          std::move(*css_content));

  auto origin = url::Origin::CreateFromNormalizedTuple(
      content::kChromeUIScheme, content::kChromeUIThemeHost, 0);
  CHECK(config->sources.find(origin) == config->sources.end());
  config->sources[origin] = std::move(source);
}
