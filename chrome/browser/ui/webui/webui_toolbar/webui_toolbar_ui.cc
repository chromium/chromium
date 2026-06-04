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
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_widget.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"
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
#include "chrome/browser/ui/webui/webui_toolbar/utils/toolbar_button_utils.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_extensions_container.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_layout_css_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/webui_toolbar_resources.h"
#include "chrome/grit/webui_toolbar_resources_map.h"
#include "components/browser_apis/browser_controls/browser_controls_api.mojom.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api.mojom.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "third_party/blink/public/common/features.h"
#include "ui/views/widget/widget.h"
#include "ui/webui/tracked_element/tracked_element_handler_document_singleton.h"
#include "ui/webui/webui_util.h"

WebUIToolbarUI::WebUIToolbarUI(content::WebUI* web_ui)
    // Sets `enable_chrome_send` to true to allow chrome.send() to be called in
    // TypeScript to record non-timestamp histograms, which can't be done by
    // MetricsReporter.
    : TopChromeWebUIController(web_ui,
                               /*enable_chrome_send=*/true,
                               /*enable_chrome_histograms=*/true),
      toolbar_channel_service_end_(
          toolbar_channel_client_end_.InitWithNewPipeAndPassReceiver()),
      browser_controls_channel_service_end_(
          browser_controls_channel_client_end_
              .InitWithNewPipeAndPassReceiver()) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIWebUIToolbarHost);

  static constexpr webui::LocalizedString kStrings[] = {
      // go/keep-sorted start
      {"backButtonAccName", IDS_ACCNAME_BACK},
      {"backButtonTooltip", IDS_TOOLTIP_BACK},
      {"forwardButtonAccName", IDS_ACCNAME_FORWARD},
      {"forwardButtonTooltip", IDS_TOOLTIP_FORWARD},
      {"homeButtonAccName", IDS_ACCNAME_HOME},
      {"homeButtonTooltip", IDS_TOOLTIP_HOME},
      {"reloadButtonAccNameReload", IDS_ACCNAME_RELOAD},
      {"reloadButtonTooltipReload", IDS_TOOLTIP_RELOAD},
      {"reloadButtonTooltipReloadWithMenu", IDS_TOOLTIP_RELOAD_WITH_MENU},
      {"reloadButtonTooltipStop", IDS_TOOLTIP_STOP},
      // go/keep-sorted end
  };
  source->AddLocalizedStrings(kStrings);

  webui::SetupWebUIDataSource(source, kWebuiToolbarResources,
                              IDR_WEBUI_TOOLBAR_WEBUI_TOOLBAR_HTML);

  WebUIToolbarLayoutCssHelper::SetAsRequestFilter(source);

  source->AddBoolean("enableReloadButton",
                     features::IsWebUIReloadButtonEnabled());
  source->AddBoolean("enableHomeButton", features::IsWebUIHomeButtonEnabled());
  source->AddBoolean("enableLocationBar",
                     features::IsWebUILocationBarEnabled());
  source->AddBoolean("enableBackForwardButtons",
                     features::IsWebUIBackForwardButtonEnabled());
  source->AddBoolean("enablePinnedToolbarActions",
                     features::IsWebUIPinnedToolbarActionsEnabled());
  source->AddBoolean("enableAppMenuButton",
                     features::IsWebUIAppMenuButtonEnabled());
  source->AddBoolean("enableAvatarButton",
                     features::IsWebUIAvatarButtonEnabled());
  source->AddBoolean("enableExtensionsContainer",
                     features::IsWebUIExtensionsContainerEnabled());
  source->AddBoolean(
      "initialWebUISurfaceSyncEnabled",
      base::FeatureList::IsEnabled(blink::features::kInitialWebUISurfaceSync));

  BrowserWindowInterface* browser =
      webui::GetBrowserWindowInterface(web_ui->GetWebContents());
  webui_toolbar::PopulateSplitTabsDataSource(source, browser);

  // Handles chrome.send() calls that records non-timestamp histograms.
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());

  if (browser) {
    // This use of unretained is safe because the
    // TrackedElementHandlerDocumentSingleton only stores the callback for at
    // most the lifetime of the WebContents, which is always shorter than the
    // Browser.
    ui::TrackedElementHandlerDocumentSingleton::Register(
        this, GetKnownElementIdentifiers(),
        base::BindRepeating(
            [](BrowserWindowInterface* browser) {
              return BrowserElements::From(browser)->GetContext();
            },
            base::Unretained(browser)));
  }
}

WEB_UI_CONTROLLER_TYPE_IMPL(WebUIToolbarUI)

WebUIToolbarUI::~WebUIToolbarUI() = default;

WebUIToolbarConfig::WebUIToolbarConfig()
    : DefaultTopChromeWebUIConfig(content::kChromeUIScheme,
                                  chrome::kChromeUIWebUIToolbarHost) {}

bool WebUIToolbarConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return features::IsWebUIToolbarEnabled() ||
         base::FeatureList::IsEnabled(
             features::kWebUIToolbarProcessOverheadExperiment);
}

bool WebUIToolbarConfig::ShouldKeepVisibleUntilFirstVisuallyNonEmptyPaint() {
  return features::kWebUIReloadButtonKeepVisibleUntilPaint.Get();
}

void WebUIToolbarUI::BindInterface(
    mojo::PendingReceiver<browser_controls_api::mojom::BrowserControlsService>
        receiver) {
  CHECK(browser_controls_channel_client_end_.is_valid())
      << "browser client end already bound";
  CHECK(FusePipes(std::move(receiver),
                  std::move(browser_controls_channel_client_end_)));
}

void WebUIToolbarUI::BindInterface(
    mojo::PendingReceiver<toolbar_ui_api::mojom::ToolbarUIService> receiver) {
  CHECK(toolbar_channel_client_end_.is_valid())
      << "toolbar client end already bound";
  CHECK(FusePipes(std::move(receiver), std::move(toolbar_channel_client_end_)));
}

void WebUIToolbarUI::BindInterface(
    mojo::PendingReceiver<extensions_bar::mojom::PageHandlerFactory> receiver) {
  extensions_bar_page_factory_receiver_.reset();
  extensions_bar_page_factory_receiver_.Bind(std::move(receiver));
}

void WebUIToolbarUI::BindInterface(
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
        receiver) {
  help_bubble_service_.reset();
  help_bubble_service_.Bind(std::move(receiver));
}

void WebUIToolbarUI::OnNavigationControlsStateChanged(
    const toolbar_ui_api::mojom::NavigationControlsState& state) {
  if (toolbar_ui_service_) {
    toolbar_ui_service_->OnNavigationControlsStateChanged(state);
  }
}

void WebUIToolbarUI::OnFocusRequested(
    toolbar_ui_api::mojom::FocusRequestTarget target) {
  if (toolbar_ui_service_) {
    toolbar_ui_service_->OnFocusRequested(target);
  }
}

void WebUIToolbarUI::Init(DependencyProvider* dependency_provider) {
  CHECK(dependency_provider);

  if (!dependency_provider->GetCommandUpdater()) {
    // If the command updater is null, the browser is likely shutting down,
    // or tearing down this specific browser window.
    // We cannot properly initialize the WebUI Toolbar without it.
    return;
  }

  InitBrowserControlsService(*dependency_provider);
  InitToolbarUIService(*dependency_provider);
}

void WebUIToolbarUI::InitBrowserControlsService(
    DependencyProvider& dependency_provider) {
  CHECK(!browser_controls_service_)
      << "Out of order initialization, the browser control service has already "
         "been instantiated.";

  auto* web_contents = web_ui()->GetWebContents();
  MetricsReporterService* metrics_service =
      MetricsReporterService::GetFromWebContents(web_contents);
  CHECK(metrics_service) << "Metrics service missing from web contents";

  browser_controls_service_ =
      std::make_unique<browser_controls_api::BrowserControlsService>(
          std::move(browser_controls_channel_service_end_),
          std::make_unique<browser_controls_api::BrowserControlsAdapterImpl>(
              webui::GetBrowserWindowInterface(web_contents),
              dependency_provider.GetCommandUpdater()),
          metrics_service->metrics_reporter(),
          dependency_provider.GetBrowserControlsDelegate());
}

void WebUIToolbarUI::InitToolbarUIService(
    DependencyProvider& dependency_provider) {
  CHECK(!toolbar_ui_service_)
      << "Out of order initialization, the toolbar UI service has already "
         "been instantiated.";

  auto* web_contents = web_ui()->GetWebContents();
  MetricsReporterService* metrics_service =
      MetricsReporterService::GetFromWebContents(web_contents);

  // If this CHECK() starts hitting, it could be due to races with browser
  // shutdown, similar to issues seen in the past (e.g., b/478033216#comment4).
  CHECK(metrics_service) << "Metrics service missing from web contents";

  toolbar_ui_service_ = std::make_unique<toolbar_ui_api::ToolbarUIService>(
      std::move(toolbar_channel_service_end_),
      dependency_provider.GetNavigationControlsStateFetcher(),
      dependency_provider.GetIconTableFetcher(),
      metrics_service->metrics_reporter(),
      dependency_provider.GetToolbarUIServiceDelegate());
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

content::WebUIController::DisplayDisposition
WebUIToolbarUI::GetDisplayDisposition() const {
  return content::WebUIController::DisplayDisposition::kUIElement;
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

void WebUIToolbarUI::CreateHelpBubbleHandler(
    mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler) {
  help_bubble_handler_ = std::make_unique<user_education::HelpBubbleHandler>(
      std::move(handler), std::move(client),
      ui::TrackedElementHandlerDocumentSingleton::GetOrCreate(
          web_ui()->GetRenderFrameHost()));
}

void WebUIToolbarUI::CreatePageHandler(
    mojo::PendingRemote<extensions_bar::mojom::Page> page,
    mojo::PendingReceiver<extensions_bar::mojom::PageHandler> receiver) {
  BrowserWindowInterface* browser_interface =
      webui::GetBrowserWindowInterface(web_ui()->GetWebContents());
  if (browser_interface) {
    static_cast<WebUIToolbarExtensionsContainer*>(
        ExtensionsContainer::From(*browser_interface))
        ->Bind(std::move(page), std::move(receiver));
  }
}

const std::vector<ui::ElementIdentifier>
WebUIToolbarUI::GetKnownElementIdentifiers() {
  static const base::NoDestructor<std::vector<ui::ElementIdentifier>> ids(
      {kLocationBarElementId, kLocationIconElementId, kOmniboxElementId,
       kReloadButtonElementId, kToolbarSplitTabsToolbarButtonElementId,
       kToolbarHomeButtonElementId, kToolbarBackButtonElementId,
       kToolbarForwardButtonElementId, kSharedTabGroupFeedbackElementId,
       kToolbarAppMenuButtonElementId, kSharedTabGroupCommentsActionElementId,
       kPinnedToolbarActionShowSidePanelLensOverlayResultsElementId,
       kPinnedToolbarActionShowSidePanelBookmarksElementId,
       kPinnedToolbarActionSendTabToSelfElementId,
       kToolbarAvatarButtonElementId,
       PermissionChipView::kPermissionRequestChipElementId,
       PermissionChipView::kIndicatorChipElementId});
  auto pinned_ids = webui_toolbar::GetPinnedToolbarActionElementIds();
  pinned_ids.reserve(pinned_ids.size() + ids->size());
  pinned_ids.insert(pinned_ids.end(), ids->begin(), ids->end());
  return pinned_ids;
}
