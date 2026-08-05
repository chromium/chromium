// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"

#include <atomic>
#include <string_view>

#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/ai_mode_button_service_factory.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_client.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter_service.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_aim_handler.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_handler.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/browser/ui/webui/sanitized_image/sanitized_image_source.h"
#include "chrome/browser/ui/webui/searchbox/omnibox_composebox_handler.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_full_handler.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/omnibox_popup_resources.h"
#include "chrome/grit/omnibox_popup_resources_map.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/lens/lens_features.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/ai_mode_button_service.h"
#include "content/public/browser/web_ui_data_source.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/webui/webui_util.h"

namespace {

using AddContextButtonVariant = omnibox::AddContextButtonVariant;

std::string_view AddContextButtonVariantToSearchboxLayoutMode(
    AddContextButtonVariant variant) {
  switch (variant) {
    case AddContextButtonVariant::kBelowResults:
      return "TallBottomContext";
    case AddContextButtonVariant::kInline:
      return "Compact";
  }

  return "";
}

void PopulateAiModeButtonUiConfig(content::WebUIDataSource* source,
                                  Profile* profile) {
  // Use AIM button service to dynamically populate the various AIM button
  // properties based on the current config, if present.
  GURL compose_icon(
      "chrome://resources/cr_components/searchbox/icons/search_spark.svg");
  if (auto* service = AiModeButtonServiceFactory::GetForProfile(profile)) {
    if (const auto* config = service->GetCurrentConfig()) {
      source->AddString("searchboxComposeButtonText", config->text);
      source->AddString("searchboxComposeButtonTitle", config->tooltip);
      source->AddString("searchboxComposeButtonA11yLabel", config->a11y_label);
      // For third-party DSE, use the favicon for the AIM button icon.
      std::string favicon_url(config->favicon_url);
      if (config->id != SearchEngineType::SEARCH_ENGINE_GOOGLE &&
          !favicon_url.empty()) {
        GURL chrome_favicon_url("chrome://favicon2/");
        chrome_favicon_url = net::AppendQueryParameter(chrome_favicon_url,
                                                       "iconUrl", favicon_url);
        chrome_favicon_url =
            net::AppendQueryParameter(chrome_favicon_url, "size", "32");
        chrome_favicon_url =
            net::AppendQueryParameter(chrome_favicon_url, "scaleFactor", "2x");
        compose_icon = chrome_favicon_url;
      }
    }
  }
  source->AddString("searchboxComposeButtonIcon", compose_icon.spec());
}

}  // namespace

bool OmniboxPopupUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return omnibox::IsAimPopupFeatureEnabled() ||
         omnibox::IsWebUIOmniboxFullPopupEnabled() ||
         omnibox::IsWebUIOmniboxPopupEnabled() ||
         base::FeatureList::IsEnabled(omnibox::kOmniboxEverywhere) ||
         features::IsWebUILocationBarEnabled();
}

bool OmniboxPopupUIConfig::ShouldCrashOnJavascriptErrorInDevelopmentBuild()
    const {
  return true;
}

OmniboxPopupUI::OmniboxPopupUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui,
                               /*enable_chrome_send=*/true,
                               /*enable_chrome_histograms=*/true),
      profile_(Profile::FromWebUI(web_ui)) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile_, chrome::kChromeUIOmniboxPopupHost);

  bool session_allows_drag_and_drop = false;
  if (auto* session_handle = GetOrCreateContextualSessionHandle()) {
    session_allows_drag_and_drop =
        session_handle->CheckSearchContentSharingSettings(profile_->GetPrefs());
  }

  source->AddLocalizedStrings(SearchboxHandler::GetWebUIDataSourceDict(
      Profile::FromWebUI(web_ui),
      {.enable_voice_search = false,
       .enable_lens_search = false,
       .session_allows_drag_and_drop = session_allows_drag_and_drop}));

  PopulateAiModeButtonUiConfig(source, profile_);

  source->AddBoolean("isTopChromeSearchbox", true);
  source->AddBoolean("isTouchUi", ui::TouchUiController::Get()->touch_ui());
  source->AddBoolean("omniboxAimPopupEnabled",
                     omnibox::IsAimPopupFeatureEnabled());
  // TODO(b/504670497): Replace this NTP-specific flag with a generic flag.
  // TODO(b/474406096): Replace this NTP-specific flag with a generic flag.
  source->AddBoolean("ntpRealboxNextEnabled", false);
  source->AddBoolean("searchboxDynamicColorScheme",
                     omnibox::kWebUIOmniboxDynamicColorScheme.Get());
  source->AddBoolean("searchboxDynamicAnimation",
                     omnibox::kWebUIOmniboxDynamicAnimation.Get());
  source->AddBoolean("omniboxShowContextButtonSuggestionLabel",
                     omnibox::kContextButtonShowSuggestionLabel.Get());
  source->AddBoolean(
      "omniboxPopupDebugEnabled",
      base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxPopupDebug));
  source->AddBoolean("webuiOmniboxPopupSelectionControlEnabled",
                     base::FeatureList::IsEnabled(
                         omnibox::kWebUIOmniboxPopupSelectionControl));
  source->AddBoolean(
      "searchboxMultiline",
      base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxFullPopup) &&
          omnibox::kWebUIOmniboxFullPopupMultiline.Get());

  source->AddBoolean("reportMetrics", true);
  source->AddString("charTypedToPaintMetricName",
                    "Omnibox.WebUI.CharTypedToRepaintLatency.ToPaint");
  source->AddString(
      "resultChangedToPaintMetricName",
      "Omnibox.Popup.WebUI.ResultChangedToRepaintLatency.ToPaint");

  // Add composebox data.
  auto composebox_config = omnibox::FeatureConfig::Get().config.composebox();
  const std::string attachment_mime_types =
      composebox_config.attachment_upload().mime_types_allowed();
  source->AddString("composeboxAttachmentFileTypes", attachment_mime_types);
  source->AddInteger("composeboxFileMaxSize",
                     composebox_config.attachment_upload().max_size_bytes());
  const std::string image_mime_types =
      composebox_config.image_upload().mime_types_allowed();
  source->AddString("composeboxImageFileTypes", image_mime_types);
  source->AddBoolean("lensSendRawFileMediaTypesEnabled",
                     lens::features::IsLensSendRawFileMediaTypesEnabled());
  source->AddBoolean(
      "caretAnimationEnabled",
      base::FeatureList::IsEnabled(omnibox::kOmniboxAnimatedCaret));
  source->AddBoolean("composeboxContextMenuEnableMultiTabSelection",
                     omnibox::kContextMenuEnableMultiTabSelection.Get());
  source->AddBoolean("composeboxShowContextMenu",
                     omnibox::kShowContextMenu.Get());
  // TODO (crbug.com/509939902) - Clean up composeboxShowContextMenuDescription
  // and determine if it should be removed in all instances.
  source->AddBoolean(
      "composeboxShowContextMenuDescription",
      omnibox::kShowContextMenuDescription.Get() &&
          omnibox::kWebUIOmniboxAimPopupAddContextButtonVariantParam.Get() !=
              omnibox::AddContextButtonVariant::kInline);
  source->AddBoolean("composeboxShowContextMenuTabPreviews",
                     omnibox::kShowContextMenuTabPreviews.Get());
  source->AddBoolean("composeboxShowImageSuggest",
                     omnibox::kShowComposeboxImageSuggestions.Get());
  source->AddBoolean("composeboxShowLensSearchChip",
                     omnibox::IsAimPopupEnabled(profile_) &&
                         omnibox::kShowLensSearchChip.Get());
  source->AddBoolean("composeboxShowCurrentTabChip",
                     omnibox::kAskGCurrentTabChip.Get());
  source->AddBoolean("composeboxShowLensIcon",
                     omnibox::kAskGLensIcon.Get());
  source->AddBoolean("askGComposeboxLensChipEnabled",
                     omnibox::kAskGComposeboxLensChip.Get());
  source->AddBoolean("composeboxShowTypedSuggest",
                     omnibox::kShowComposeboxTypedSuggest.Get());
  source->AddBoolean("composeboxShowZps", omnibox::kShowComposeboxZps.Get());
  source->AddBoolean("composeboxSmartComposeEnabled",
                     omnibox::kShowSmartCompose.Get());
  source->AddBoolean("contextButtonHasBackground",
                     omnibox::kContextButtonHasBackground.Get());
  source->AddBoolean("webuiOmniboxSimplificationEnabled",
                     base::FeatureList::IsEnabled(
                         omnibox::internal::kWebUIOmniboxSimplification));
  source->AddBoolean("hideClassicContextButton",
                     omnibox::kHideClassicContextButton.Get());
  source->AddBoolean(
      "contextManagementInComposeboxEnabled",
      base::FeatureList::IsEnabled(omnibox::kContextManagementInComposebox) &&
          base::FeatureList::IsEnabled(omnibox::kContextManagementInOmnibox));
  source->AddBoolean(
      "composeboxSkillsEnabled",
      base::FeatureList::IsEnabled(omnibox::kComposeboxSkillsOmniboxPopup));
  source->AddBoolean(
      "tabFaviconChipsToCoinsEnabled",
      base::FeatureList::IsEnabled(omnibox::kContextManagementInComposebox) &&
          base::FeatureList::IsEnabled(omnibox::kContextManagementInOmnibox) &&
          base::FeatureList::IsEnabled(omnibox::kTabFaviconChipsToCoins));
  auto searchbox_layout_mode = AddContextButtonVariantToSearchboxLayoutMode(
      omnibox::kWebUIOmniboxAimPopupAddContextButtonVariantParam.Get());
  source->AddString("searchboxLayoutMode", searchbox_layout_mode);
  source->AddString(
      "composeboxSource",
      contextual_search::ContextualSearchMetricsRecorder::
          ContextualSearchSourceToString(
              contextual_search::ContextualSearchSource::kOmnibox));
  source->AddBoolean("caretColorAnimationDisabled",
                     base::FeatureList::IsEnabled(
                         omnibox::kWebUIOmniboxDisableCaretColorAnimation));
  source->AddBoolean("composeboxAnimationDisabled",
                     base::FeatureList::IsEnabled(
                         omnibox::kWebUIOmniboxAimPopupDisableAnimation));
  source->AddBoolean(
      "energyEffectEnabled",
      base::FeatureList::IsEnabled(omnibox::kEnergyEffectInOmnibox));
  source->AddBoolean(
      "energyEffectAnimationEnabled",
      base::FeatureList::IsEnabled(omnibox::kEnergyEffectInOmnibox));
  source->AddBoolean("contextButtonShapeIsOblong",
                     omnibox::kContextButtonShapeIsOblong.Get());

  int default_resource = IDR_OMNIBOX_POPUP_OMNIBOX_POPUP_HTML;
  if (omnibox::IsWebUIOmniboxFullPopupEnabled()) {
    default_resource = IDR_OMNIBOX_POPUP_OMNIBOX_POPUP_FULL_HTML;
  }
  webui::SetupWebUIDataSource(source, kOmniboxPopupResources, default_resource);
  webui::EnableTrustedTypesCSP(source);

  // Add a handler to provide pluralized strings.
  auto plural_string_handler = std::make_unique<PluralStringHandler>();
  plural_string_handler->AddLocalizedString("sharingTabs",
                                            IDS_COMPOSE_SHARING_TABS);
  web_ui->AddMessageHandler(std::move(plural_string_handler));

  content::URLDataSource::Add(profile_,
                              std::make_unique<SanitizedImageSource>(profile_));
  content::URLDataSource::Add(
      profile_, std::make_unique<FaviconSource>(
                    profile_, chrome::FaviconUrlFormat::kFavicon2));
}

OmniboxPopupUI::~OmniboxPopupUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(OmniboxPopupUI)

void OmniboxPopupUI::BindInterface(
    content::RenderFrameHost* host,
    mojo::PendingReceiver<searchbox::mojom::PageHandlerFactory>
        pending_page_handler) {
  if (searchbox_page_factory_receiver_.is_bound()) {
    searchbox_page_factory_receiver_.reset();
  }
  searchbox_page_factory_receiver_.Bind(std::move(pending_page_handler));
}

void OmniboxPopupUI::CreatePageHandler(
    mojo::PendingRemote<searchbox::mojom::Page> page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler) {
  auto* omnibox_controller =
      OmniboxPopupWebContentsHelper::GetOrCreateForWebContents(
          web_ui()->GetWebContents())
          ->get_omnibox_controller();
  CHECK(omnibox_controller);

  MetricsReporterService* metrics_reporter_service =
      MetricsReporterService::GetFromWebContents(web_ui()->GetWebContents());
  if (omnibox::ShouldUseWebUIOmniboxFullHandler()) {
    ChromeOmniboxClient* client =
        static_cast<ChromeOmniboxClient*>(omnibox_controller->client());
    CHECK(client);
    omnibox_handler_ = std::make_unique<WebuiOmniboxFullHandler>(
        std::move(pending_page_handler), std::move(page),
        Profile::FromWebUI(web_ui()), web_ui()->GetWebContents(),
        std::make_unique<ChromeOmniboxClient>(
            client->GetLocationBar(), client->browser(), client->profile()),
        base::BindRepeating(&OmniboxPopupUI::GetOrCreateContextualSessionHandle,
                            base::Unretained(this)));
  } else {
    omnibox_handler_ = std::make_unique<WebuiOmniboxHandler>(
        std::move(pending_page_handler), std::move(page),
        metrics_reporter_service->metrics_reporter(), omnibox_controller,
        web_ui(),
        base::BindRepeating(&OmniboxPopupUI::GetOrCreateContextualSessionHandle,
                            base::Unretained(this)));
  }
}

void OmniboxPopupUI::BindInterface(
    mojo::PendingReceiver<omnibox_popup::mojom::PageHandlerFactory> receiver) {
  popup_page_factory_receiver_.reset();
  popup_page_factory_receiver_.Bind(std::move(receiver));
}

void OmniboxPopupUI::CreatePageHandler(
    mojo::PendingRemote<omnibox_popup::mojom::Page> page,
    mojo::PendingReceiver<omnibox_popup::mojom::PageHandler> receiver) {
  auto* omnibox_controller =
      OmniboxPopupWebContentsHelper::GetOrCreateForWebContents(
          web_ui()->GetWebContents())
          ->get_omnibox_controller();
  CHECK(omnibox_controller);

  popup_handler_ = std::make_unique<OmniboxPopupHandler>(
      std::move(receiver), std::move(page), web_ui()->GetWebContents(),
      omnibox_controller);
  popup_handler_->set_embedder(embedder());
}

void OmniboxPopupUI::BindInterface(
    mojo::PendingReceiver<omnibox_popup_aim::mojom::PageHandlerFactory>
        receiver) {
  aim_page_factory_receiver_.reset();
  aim_page_factory_receiver_.Bind(std::move(receiver));
}

void OmniboxPopupUI::CreatePageHandler(
    mojo::PendingRemote<omnibox_popup_aim::mojom::Page> page,
    mojo::PendingReceiver<omnibox_popup_aim::mojom::PageHandler> receiver) {
  popup_aim_handler_ = std::make_unique<OmniboxPopupAimHandler>(
      std::move(receiver), std::move(page), web_ui()->GetWebContents());
  popup_aim_handler_->set_embedder(embedder());
}

void OmniboxPopupUI::BindInterface(
    mojo::PendingReceiver<composebox::mojom::PageHandlerFactory> receiver) {
  if (!omnibox::IsAimPopupFeatureEnabled()) {
    return;
  }
  if (composebox_page_factory_receiver_.is_bound()) {
    composebox_page_factory_receiver_.reset();
  }
  composebox_page_factory_receiver_.Bind(std::move(receiver));
}

void OmniboxPopupUI::CreatePageHandler(
    mojo::PendingReceiver<composebox::mojom::PageHandler> pending_page_handler,
    mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler>
        pending_searchbox_handler) {
  composebox_handler_ = std::make_unique<OmniboxComposeboxHandler>(
      std::move(pending_page_handler), std::move(pending_searchbox_handler),
      std::move(pending_searchbox_page), profile_, web_ui()->GetWebContents(),
      base::BindRepeating(&OmniboxPopupUI::GetOrCreateContextualSessionHandle,
                          base::Unretained(this)),
      base::BindRepeating(&OmniboxPopupUI::ClearContextualSessionHandle,
                          base::Unretained(this)));

  // If presenter delegate is connected, set the delegate in the handler so the
  // handler can notify the delegate when there are embedded permissions prompt
  // changes. Otherwise, once the presenter delegate connects itself, connect
  // the two there instead. `composebox_handler_` is used since composebox page
  // handler is where PEPC mojo call lives.
  if (presenter_delegate_) {
    composebox_handler_->set_delegate(presenter_delegate_);
  }
}

contextual_search::ContextualSearchSessionHandle*
OmniboxPopupUI::GetOrCreateContextualSessionHandle() {
  if (!shared_session_handle_) {
    auto* contextual_search_service =
        ContextualSearchServiceFactory::GetForProfile(profile_);
    if (contextual_search_service) {
      shared_session_handle_ = contextual_search_service->CreateSession(
          omnibox::CreateQueryControllerConfigParams(),
          contextual_search::ContextualSearchSource::kOmnibox,
          lens::LensOverlayInvocationSource::kOmniboxContextualQuery);
      shared_session_handle_->CheckSearchContentSharingSettings(
          Profile::FromWebUI(web_ui())->GetPrefs());
    }
  }
  return shared_session_handle_.get();
}

void OmniboxPopupUI::ClearContextualSessionHandle() {
  shared_session_handle_.reset();
}

void OmniboxPopupUI::SetPresenterDelegate(OmniboxPopupPresenterBase* delegate) {
  presenter_delegate_ = delegate;

  // If the handler is initialized already, set the delegate in the handler so
  // the handler can notify the delegate when there are embedded permissions
  // prompt changes. Otherwise, once the handler is initialized, connect the two
  // there instead. `composebox_handler_` is used since composebox page handler
  // is where PEPC mojo call lives.
  if (composebox_handler_) {
    composebox_handler_->set_delegate(presenter_delegate_);
  }
}
