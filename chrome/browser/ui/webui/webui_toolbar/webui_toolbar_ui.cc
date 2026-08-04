// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_ui.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_widget.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"
#include "chrome/browser/ui/views/toolbar/avatar_toolbar_button_interface.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter_service.h"
#include "chrome/browser/ui/webui/theme_colors_source_manager.h"
#include "chrome/browser/ui/webui/theme_colors_source_manager_factory.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/ui/webui/webui_toolbar/adapters/browser_controls_adapter_impl.h"
#include "chrome/browser/ui/webui/webui_toolbar/adapters/navigation_controls_state_fetcher_impl.h"
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
#include "chrome/grit/webui_toolbar_shared_resources.h"
#include "chrome/grit/webui_toolbar_shared_resources_map.h"
#include "components/browser_apis/browser_controls/browser_controls_api.mojom.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api.mojom.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/widget/widget.h"
#include "ui/webui/tracked_element/tracked_element_handler_document_singleton.h"
#include "ui/webui/webui_util.h"

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebUIToolbarUIDependencyProviderUserData);

WebUIToolbarUIDependencyProviderUserData::
    WebUIToolbarUIDependencyProviderUserData(
        content::WebContents* contents,
        WebUIToolbarUI::DependencyProvider* provider)
    : content::WebContentsUserData<WebUIToolbarUIDependencyProviderUserData>(
          *contents),
      provider_(provider ? provider->GetWeakPtr() : nullptr) {}

WebUIToolbarUIDependencyProviderUserData::
    ~WebUIToolbarUIDependencyProviderUserData() = default;

namespace {

constexpr char kIsNavigationLoading[] = "isNavigationLoading";
constexpr char kReloadCanShowMenu[] = "reloadCanShowMenu";
constexpr char kBackButtonEnabled[] = "backButtonEnabled";
constexpr char kForwardButtonEnabled[] = "forwardButtonEnabled";
constexpr char kHomeButtonShouldBeShown[] = "homeButtonShouldBeShown";
constexpr char kBatterySaverButtonVisible[] = "batterySaverButtonVisible";
constexpr char kLayoutConstantsVersion[] = "layoutConstantsVersion";
constexpr char kTouchUi[] = "touchUi";
constexpr char kInitialWebUISurfaceSyncEnabled[] =
    "initialWebUISurfaceSyncEnabled";
constexpr char kIsFallbackPrewarming[] = "isFallbackPrewarming";

// Retrieves the current navigation controls state from the provider's fetcher
// and populates a nested dictionary `initialState`.
//
// This method extracts only the critical subset of `NavigationControlsState`
// required for the initial paint of the toolbar on startup. This critical
// subset includes reload, back, forward, and home button states, touch UI mode,
// battery saver visibility, and layout constants version. These fields are
// required immediately during TypeScript construction to prevent visual layout
// shifts or delayed rendering. Other non-critical states are omitted here and
// will be updated later asynchronously via Mojo.
//
// The populated nested dictionary is serialized and passed to the frame using
// `SetWebUIProperty("initialState", json_string)`. This allows the frontend to
// access the initial state synchronously via
// `chrome.getVariableValue('initialState')` during load.
//
// If provider is null, such as during background prewarming when the actual
// browser window view is not yet created, it directly populates default
// dictionary values.
// TODO(crbug.com/530370659): Replaced with mojom struct for type safety.
void PopulateInitialState(base::DictValue& dict,
                          WebUIToolbarUI::DependencyProvider* provider) {
  dict.Set(
      kInitialWebUISurfaceSyncEnabled,
      base::FeatureList::IsEnabled(blink::features::kInitialWebUISurfaceSync));
  toolbar_ui_api::mojom::NavigationControlsStatePtr state;
  if (provider && provider->GetNavigationControlsStateFetcher()) {
    state = provider->GetNavigationControlsStateFetcher()
                ->GetNavigationControlsState();
  }
  if (!state) {
    dict.Set(kIsNavigationLoading, false);
    dict.Set(kReloadCanShowMenu, false);
    dict.Set(kBackButtonEnabled, false);
    dict.Set(kForwardButtonEnabled, false);
    dict.Set(kHomeButtonShouldBeShown, false);
    dict.Set(kBatterySaverButtonVisible, false);
    dict.Set(kLayoutConstantsVersion, 0);
    dict.Set(kTouchUi, false);
    return;
  }

  if (state->reload_control_state) {
    dict.Set(kIsNavigationLoading,
             state->reload_control_state->is_navigation_loading);
    dict.Set(kReloadCanShowMenu, state->reload_control_state->can_show_menu);
  } else {
    dict.Set(kIsNavigationLoading, false);
    dict.Set(kReloadCanShowMenu, false);
  }
  if (state->back_forward_control_state) {
    dict.Set(kBackButtonEnabled,
             state->back_forward_control_state->back_button_state &&
                 state->back_forward_control_state->back_button_state->enabled);
    dict.Set(
        kForwardButtonEnabled,
        state->back_forward_control_state->forward_button_state &&
            state->back_forward_control_state->forward_button_state->enabled);
  } else {
    dict.Set(kBackButtonEnabled, false);
    dict.Set(kForwardButtonEnabled, false);
  }
  if (state->home_control_state) {
    dict.Set(kHomeButtonShouldBeShown,
             state->home_control_state->should_be_shown);
  } else {
    dict.Set(kHomeButtonShouldBeShown, false);
  }
  dict.Set(kBatterySaverButtonVisible, state->battery_saver_button_visible);
  dict.Set(kLayoutConstantsVersion, state->layout_constants_version);
  dict.Set(kTouchUi, state->touch_ui);
}

}  // namespace

WebUIToolbarUI::WebUIToolbarUI(content::WebUI* web_ui)
    // Sets `enable_chrome_send` to true to allow chrome.send() to be called in
    // TypeScript to record non-timestamp histograms, which can't be done by
    // MetricsReporter.
    : TopChromeWebUIController(web_ui,
                               /*enable_chrome_send=*/true,
                               /*enable_chrome_histograms=*/true),
      content::WebContentsObserver(web_ui->GetWebContents()),
      toolbar_channel_service_end_(
          toolbar_channel_client_end_.InitWithNewPipeAndPassReceiver()),
      browser_controls_channel_service_end_(
          browser_controls_channel_client_end_
              .InitWithNewPipeAndPassReceiver()) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIWebUIToolbarHost);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));

  static constexpr webui::LocalizedString kStrings[] = {
      // go/keep-sorted start
      {"backButtonAccName", IDS_ACCNAME_BACK},
      {"backButtonTooltip", IDS_TOOLTIP_BACK},
      {"batterySaverButtonAccName", IDS_BATTERY_SAVER_BUTTON_ACCNAME},
      {"batterySaverButtonTooltip", IDS_BATTERY_SAVER_BUTTON_TOOLTIP},
      {"forwardButtonAccName", IDS_ACCNAME_FORWARD},
      {"forwardButtonTooltip", IDS_TOOLTIP_FORWARD},
      {"homeButtonAccName", IDS_ACCNAME_HOME},
      {"homeButtonTooltip", IDS_TOOLTIP_HOME},
      {"performanceInterventionButtonAccName",
       IDS_PERFORMANCE_INTERVENTION_BUTTON_ACCNAME},
      {"performanceInterventionButtonTooltip",
       IDS_PERFORMANCE_INTERVENTION_BUTTON_TOOLTIP},
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

  source->AddBoolean("roundedIconsEnabled", features::IsRoundedIconsEnabled());
  source->AddBoolean("enableReloadButton",
                     features::IsWebUIReloadButtonEnabled());
  source->AddBoolean("enableHomeButton", features::IsWebUIHomeButtonEnabled());
  source->AddBoolean("enableBatterySaverButton",
                     features::IsWebUIBatterySaverButtonEnabled());
  source->AddBoolean("enableLocationBar",
                     features::IsWebUILocationBarEnabled());
  source->AddBoolean("enableBackForwardButtons",
                     features::IsWebUIBackForwardButtonEnabled());
  source->AddBoolean("enablePinnedToolbarActions",
                     features::IsWebUIPinnedToolbarActionsEnabled());
  source->AddBoolean("enableAppMenuButton",
                     features::IsWebUIAppMenuButtonEnabled());
  source->AddBoolean(
      "enableAvatarButton",
      features::IsWebUIAvatarButtonEnabled() &&
          AvatarToolbarButtonInterface::CanShowForProfile(profile));
  source->AddBoolean("enableExtensionsContainer",
                     features::IsWebUIExtensionsContainerEnabled());
  source->AddBoolean("enablePerformanceInterventionButton",
                     features::IsWebUIPerformanceInterventionButtonEnabled());
  source->AddBoolean(
      "initialWebUISurfaceSyncEnabled",
      base::FeatureList::IsEnabled(blink::features::kInitialWebUISurfaceSync));
  source->AddBoolean(
      "omniboxResizingPrioritizationEnabled",
      base::FeatureList::IsEnabled(features::kOmniboxResizingPrioritization));
  source->AddBoolean("webUIToolbarFullyEnabled",
                     features::IsWebUIToolbarFullyEnabled());

  BrowserWindowInterface* browser =
      webui::GetBrowserWindowInterface(web_ui->GetWebContents());
  webui_toolbar::PopulateSplitTabsDataSource(source, browser);

  source->AddResourcePaths(kWebuiToolbarSharedResources);

  // Handles chrome.send() calls that records non-timestamp histograms.
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());

  if (browser) {
    auto context = BrowserElements::From(browser)->GetContext();
    ui::TrackedElementHandlerDocumentSingleton::Register(
        this, GetKnownElementIdentifiers(),
        context ? base::BindRepeating([](ui::ElementContext c) { return c; },
                                      context)
                : base::RepeatingCallback<ui::ElementContext()>());
  }

  Profile* profile_ptr = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(
      profile_ptr, std::make_unique<FaviconSource>(
                       profile_ptr, chrome::FaviconUrlFormat::kFavicon2));
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

  browser_controls_service_ =
      std::make_unique<browser_controls_api::BrowserControlsService>(
          std::move(browser_controls_channel_service_end_),
          std::make_unique<browser_controls_api::BrowserControlsAdapterImpl>(
              webui::GetBrowserWindowInterface(web_contents),
              dependency_provider.GetCommandUpdater(), web_contents),
          dependency_provider.GetBrowserControlsDelegate(),
          web_ui()->GetRenderFrameHost());
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

void WebUIToolbarUI::WebUIRenderFrameCreated(content::RenderFrameHost* rfh) {
  TopChromeWebUIController::WebUIRenderFrameCreated(rfh);

  // Set the custom timeout for WebUI toolbar renderer to restart on
  // unresponsiveness.
  if (features::kWebUIReloadButtonRestartUnresponsive.Get()) {
    rfh->GetRenderWidgetHost()->SetHungRendererDelay(
        features::kWebUIReloadButtonRestartUnresponsiveRenderersTimeout.Get());
  }

  // Inject the initial toolbar state synchronously into the frame to allow
  // the JS frontend to initialize synchronously during load.
  //
  // During Pre-Navigate prewarming, the window View does not exist yet when the
  // page starts loading in the background, so the UserData is missing. In this
  // case, we instantiate a local FallbackDependencyProvider to inject a safe,
  // default-valued initial state.
  //
  // Otherwise (normal load or Renderer-Only prewarming), we retrieve the actual
  // window View's provider from UserData.
  auto* user_data = WebUIToolbarUIDependencyProviderUserData::FromWebContents(
      web_ui()->GetWebContents());
  base::DictValue values;
  if (user_data) {
    auto* provider_user_data = user_data->provider();
    PopulateInitialState(values, provider_user_data);
    values.Set(kIsFallbackPrewarming, false);
  } else {
    PopulateInitialState(values, nullptr);
    values.Set(kIsFallbackPrewarming, true);
  }
  std::string json_string;
  base::JSONWriter::Write(values, &json_string);
  rfh->SetWebUIProperty("initialState", json_string);
}

content::WebUIController::DisplayDisposition
WebUIToolbarUI::GetDisplayDisposition() const {
  return content::WebUIController::DisplayDisposition::kUIElement;
}

void WebUIToolbarUI::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->HasCommitted() &&
      navigation_handle->IsInPrimaryMainFrame() &&
      !navigation_handle->IsSameDocument()) {
    // Cache the navigation start time. This is the single source of truth
    // for the active document's time origin.
    navigation_start_ticks_ = navigation_handle->NavigationStart();
  }
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

const std::vector<ui::ElementIdentifier>
WebUIToolbarUI::GetKnownElementIdentifiers() {
  static const base::NoDestructor<std::vector<ui::ElementIdentifier>> ids(
      {kLocationBarElementId,
       kLocationIconElementId,
       kOmniboxElementId,
       kReloadButtonElementId,
       kToolbarSplitTabsToolbarButtonElementId,
       kToolbarHomeButtonElementId,
       kToolbarBackButtonElementId,
       kToolbarForwardButtonElementId,
       kSharedTabGroupFeedbackElementId,
       kToolbarAppMenuButtonElementId,
       kSharedTabGroupCommentsActionElementId,
       kPinnedToolbarActionShowSidePanelLensOverlayResultsElementId,
       kPinnedToolbarActionShowSidePanelBookmarksElementId,
       kPinnedToolbarActionShowSidePanelContextualTasksElementId,
       kPinnedToolbarActionSendTabToSelfElementId,
       kToolbarAvatarButtonElementId,
       kToolbarPerformanceInterventionButtonElementId,
       PermissionChipView::kPermissionRequestChipElementId,
       PermissionChipView::kIndicatorChipElementId,
       kToolbarBatterySaverButtonElementId,
       kExtensionsMenuButtonElementId,
       kToolbarActionViewElementId});
  auto pinned_ids = webui_toolbar::GetPinnedToolbarActionElementIds();
  pinned_ids.reserve(pinned_ids.size() + ids->size());
  pinned_ids.insert(pinned_ids.end(), ids->begin(), ids->end());
  return pinned_ids;
}
