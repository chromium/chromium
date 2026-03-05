// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_ui.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_widget.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter_service.h"
#include "chrome/browser/ui/webui/theme_colors_source_manager.h"
#include "chrome/browser/ui/webui/theme_colors_source_manager_factory.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/ui/webui/webui_toolbar/adapters/browser_controls_adapter_impl.h"
#include "chrome/browser/ui/webui/webui_toolbar/browser_controls_service.h"
#include "chrome/browser/ui/webui/webui_toolbar/toolbar_ui_service.h"
#include "chrome/browser/ui/webui/webui_toolbar/utils/split_tabs_utils.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_layout_css_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/webui_toolbar_resources.h"
#include "chrome/grit/webui_toolbar_resources_map.h"
#include "components/browser_apis/browser_controls/browser_controls_api.mojom.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api.mojom.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/widget/widget.h"
#include "ui/webui/tracked_element/tracked_element_handler.h"
#include "ui/webui/webui_util.h"

WebUIToolbarUI::WebUIToolbarUI(content::WebUI* web_ui)
    // Sets `enable_chrome_send` to true to allow chrome.send() to be called in
    // TypeScript to record non-timestamp histograms, which can't be done by
    // MetricsReporter.
    : TopChromeWebUIController(web_ui,
                               /*enable_chrome_send=*/true,
                               /*enable_chrome_histograms=*/true) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIWebUIToolbarHost);

  static constexpr webui::LocalizedString kStrings[] = {
      // go/keep-sorted start
      {"reloadButtonAccNameReload", IDS_ACCNAME_RELOAD},
      {"reloadButtonTooltipReload", IDS_TOOLTIP_RELOAD},
      {"reloadButtonTooltipReloadWithMenu", IDS_TOOLTIP_RELOAD_WITH_MENU},
      {"reloadButtonTooltipStop", IDS_TOOLTIP_STOP},
      // go/keep-sorted end
  };
  source->AddLocalizedStrings(kStrings);

  const auto& typography_provider = views::TypographyProvider::Get();
  AddFontVariables("omniboxPrimary",
                   typography_provider.GetFont(CONTEXT_OMNIBOX_PRIMARY,
                                               views::style::STYLE_PRIMARY),
                   source);

  webui::SetupWebUIDataSource(source, kWebuiToolbarResources,
                              IDR_WEBUI_TOOLBAR_WEBUI_TOOLBAR_HTML);

  WebUIToolbarLayoutCssHelper::SetAsRequestFilter(source);

  source->AddBoolean("enableReloadButton",
                     features::IsWebUIReloadButtonEnabled());
  source->AddBoolean("enableLocationBar",
                     features::IsWebUILocationBarEnabled());

  BrowserWindowInterface* browser =
      webui::GetBrowserWindowInterface(web_ui->GetWebContents());
  webui_toolbar::PopulateSplitTabsDataSource(source, browser);

  // Handles chrome.send() calls that records non-timestamp histograms.
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());
}

WEB_UI_CONTROLLER_TYPE_IMPL(WebUIToolbarUI)

WebUIToolbarUI::~WebUIToolbarUI() = default;

WebUIToolbarConfig::WebUIToolbarConfig()
    : DefaultTopChromeWebUIConfig(content::kChromeUIScheme,
                                  chrome::kChromeUIWebUIToolbarHost) {}

bool WebUIToolbarConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return features::IsWebUIToolbarEnabled();
}

void WebUIToolbarUI::BindInterface(
    mojo::PendingReceiver<browser_controls_api::mojom::BrowserControlsService>
        receiver) {
  CHECK(dependency_provider_)
      << "Dependency provider is not set, make sure to call Init() first";

  auto* command_updater = GetCommandUpdater();
  auto is_probably_shutting_down = command_updater == nullptr;
  if (is_probably_shutting_down) {
    LOG(WARNING) << "Attempting a connection when the browser is probably "
                    "shutting down. Aborting Bind.";
    return;
  }

  auto* web_contents = web_ui()->GetWebContents();
  MetricsReporterService* metrics_service =
      MetricsReporterService::GetFromWebContents(web_contents);
  CHECK(metrics_service) << "Metrics service missing from web contents";

  browser_controls_service_ =
      std::make_unique<browser_controls_api::BrowserControlsService>(
          std::move(receiver),
          std::make_unique<browser_controls_api::BrowserControlsAdapterImpl>(
              webui::GetBrowserWindowInterface(web_contents), command_updater),
          metrics_service->metrics_reporter(),
          dependency_provider_->GetBrowserControlsDelegate());
}

void WebUIToolbarUI::BindInterface(
    mojo::PendingReceiver<toolbar_ui_api::mojom::ToolbarUIService> receiver) {
  CHECK(dependency_provider_)
      << "Dependency provider is not set, make sure to call Init() first";

  auto* web_contents = web_ui()->GetWebContents();
  MetricsReporterService* metrics_service =
      MetricsReporterService::GetFromWebContents(web_contents);

  // If this CHECK() starts hitting, it could be due to races with browser
  // shutdown, similar to issues seen in the past (e.g., b/478033216#comment4).
  CHECK(metrics_service) << "Metrics service missing from web contents";

  toolbar_ui_service_ = std::make_unique<toolbar_ui_api::ToolbarUIService>(
      std::move(receiver),
      dependency_provider_->GetNavigationControlsStateFetcher(),
      metrics_service->metrics_reporter(),
      dependency_provider_->GetToolbarUIServiceDelegate());
}

void WebUIToolbarUI::BindInterface(
    mojo::PendingReceiver<tracked_element::mojom::TrackedElementHandler>
        receiver) {
  BrowserWindowInterface* browser_interface =
      webui::GetBrowserWindowInterface(web_ui()->GetWebContents());
  if (browser_interface) {
    ui::ElementContext element_context =
        BrowserElements::From(browser_interface)->GetContext();

    tracked_element_handler_ = std::make_unique<ui::TrackedElementHandler>(
        web_ui()->GetWebContents(), std::move(receiver), element_context,
        GetKnownElementIdentifiers());
  }
}

void WebUIToolbarUI::OnNavigationControlsStateChanged(
    toolbar_ui_api::mojom::NavigationControlsStatePtr state) {
  if (toolbar_ui_service_) {
    toolbar_ui_service_->OnNavigationControlsStateChanged(std::move(state));
  }
}

void WebUIToolbarUI::Init(DependencyProvider* dependency_provider) {
  CHECK(!browser_controls_service_)
      << "Out of order initialization, the browser control service has already "
         "been instantiated.";

  CHECK(!toolbar_ui_service_)
      << "Out of order initialization, the toolbar UI service has already "
         "been instantiated.";

  dependency_provider_ = dependency_provider;
}

CommandUpdater* WebUIToolbarUI::GetCommandUpdater() const {
  BrowserWindowInterface* browser_interface =
      webui::GetBrowserWindowInterface(web_ui()->GetWebContents());
  if (!browser_interface) {
    return nullptr;
  }
  return browser_interface->GetFeatures().browser_command_controller();
}

void WebUIToolbarUI::WebUIRenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  TopChromeWebUIController::WebUIRenderFrameCreated(render_frame_host);

  // Set the custom timeout for WebUI toolbar renderer to restart on
  // unresponsiveness.
  if (features::kWebUIReloadButtonRestartUnresponsive.Get()) {
    render_frame_host->GetRenderWidgetHost()->SetHungRendererDelay(
        features::kWebUIReloadButtonRestartUnresponsiveRenderersTimeout.Get());
  }
}

// static
void WebUIToolbarUI::AddFontVariables(std::string_view prefix,
                                      const gfx::FontList& font,
                                      content::WebUIDataSource* source) {
  DCHECK_EQ(1u, font.GetFonts().size());
  source->AddString(base::StrCat({prefix, "Family"}),
                    font.GetPrimaryFont().GetFontName());
  source->AddInteger(base::StrCat({prefix, "Size"}), font.GetFontSize());
  source->AddInteger(base::StrCat({prefix, "Weight"}),
                     static_cast<int>(font.GetFontWeight()));
}

void WebUIToolbarUI::PopulateLocalResourceLoaderConfig(
    blink::mojom::LocalResourceLoaderConfig* config,
    const url::Origin& requesting_origin) {
  auto* theme_colors_manager = ThemeColorsSourceManagerFactory::GetForProfile(
      Profile::FromWebUI(web_ui()));
  CHECK(theme_colors_manager);
  theme_colors_manager->PopulateLocalResourceLoaderConfig(
      config, requesting_origin, web_ui()->GetWebContents());

  WebUIToolbarLayoutCssHelper::PopulateLocalResourceLoaderConfig(config);
}

const std::vector<ui::ElementIdentifier>
WebUIToolbarUI::GetKnownElementIdentifiers() const {
  return {kReloadButtonElementId, kToolbarSplitTabsToolbarButtonElementId};
}
