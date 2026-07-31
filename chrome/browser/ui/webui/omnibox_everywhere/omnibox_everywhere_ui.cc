// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_everywhere/omnibox_everywhere_ui.h"

#include "base/feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service_factory.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_handler.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter_service.h"
#include "chrome/browser/ui/webui/omnibox_everywhere/composebox_everywhere_handler.h"
#include "chrome/browser/ui/webui/omnibox_everywhere/omnibox_everywhere_handler.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/browser/ui/webui/sanitized_image/sanitized_image_source.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/omnibox_everywhere_resources.h"
#include "chrome/grit/omnibox_everywhere_resources_map.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/lens/lens_features.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/webui/webui_util.h"

namespace {

bool IsAimEligible(Profile* profile) {
  auto* aim_eligibility_service =
      AimEligibilityServiceFactory::GetForProfile(profile);
  return aim_eligibility_service && aim_eligibility_service->IsAimEligible();
}

bool IsFuseboxEligible(Profile* profile) {
  return IsAimEligible(profile) &&
         AimEligibilityServiceFactory::GetForProfile(profile)
             ->IsFuseboxEligible();
}

}  // namespace

bool OmniboxEverywhereUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(omnibox::kOmniboxEverywhere);
}

bool OmniboxEverywhereUIConfig::ShouldCrashOnJavascriptErrorInDevelopmentBuild()
    const {
  return true;
}

OmniboxEverywhereUI::OmniboxEverywhereUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui,
                               /*enable_chrome_send=*/true,
                               /*enable_chrome_histograms=*/true),
      profile_(Profile::FromWebUI(web_ui)) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile_, chrome::kChromeUIOmniboxEverywhereHost);

  webui::SetupWebUIDataSource(source, kOmniboxEverywhereResources,
                              IDR_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_HTML);

  // Sanitized image and favicon source initialization
  content::URLDataSource::Add(profile_,
                              std::make_unique<SanitizedImageSource>(profile_));
  content::URLDataSource::Add(
      profile_, std::make_unique<FaviconSource>(
                    profile_, chrome::FaviconUrlFormat::kFavicon2));

  bool session_allows_drag_and_drop = false;
  if (auto* session_handle = GetOrCreateContextualSessionHandle()) {
    session_allows_drag_and_drop =
        session_handle->CheckSearchContentSharingSettings(profile_->GetPrefs());
  }

  // Configure WebUIDataSource dictionary
  source->AddLocalizedStrings(SearchboxHandler::GetWebUIDataSourceDict(
      profile_,
      {.enable_voice_search = true,
       .enable_lens_search = true,
       .session_allows_drag_and_drop = session_allows_drag_and_drop}));

  source->AddBoolean("isTopChromeSearchbox", false);
  source->AddBoolean("isTouchUi", ui::TouchUiController::Get()->touch_ui());
  source->AddBoolean("omniboxAimPopupEnabled",
                     omnibox::IsAimPopupFeatureEnabled());
  source->AddBoolean("omniboxShowContextButtonSuggestionLabel",
                     omnibox::kContextButtonShowSuggestionLabel.Get());
  source->AddBoolean(
      "omniboxPopupDebugEnabled",
      base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxPopupDebug));
  source->AddBoolean("webuiOmniboxPopupSelectionControlEnabled",
                     base::FeatureList::IsEnabled(
                         omnibox::kWebUIOmniboxPopupSelectionControl));

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

  source->AddBoolean("searchboxShowComposeEntrypoint", IsAimEligible(profile_));
  source->AddBoolean("ntpRealboxNextEnabled", IsFuseboxEligible(profile_));
  source->AddBoolean("searchboxLensSearch", true);

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
      "tabFaviconChipsToCoinsEnabled",
      base::FeatureList::IsEnabled(omnibox::kContextManagementInComposebox) &&
          base::FeatureList::IsEnabled(omnibox::kContextManagementInOmnibox) &&
          base::FeatureList::IsEnabled(omnibox::kTabFaviconChipsToCoins));
  source->AddBoolean(
      "composeboxSkillsEnabled",
      base::FeatureList::IsEnabled(omnibox::kComposeboxSkillsOmniboxEverywhere));

  source->AddString("searchboxLayoutMode", "TallBottomContext");
  source->AddString(
      "composeboxSource",
      contextual_search::ContextualSearchMetricsRecorder::
          ContextualSearchSourceToString(
              contextual_search::ContextualSearchSource::kOmniboxEverywhere));
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

  // Add a handler to provide pluralized strings.
  auto plural_string_handler = std::make_unique<PluralStringHandler>();
  plural_string_handler->AddLocalizedString("sharingTabs",
                                            IDS_COMPOSE_SHARING_TABS);
  web_ui->AddMessageHandler(std::move(plural_string_handler));
}

OmniboxEverywhereUI::~OmniboxEverywhereUI() = default;

void OmniboxEverywhereUI::BindInterface(
    mojo::PendingReceiver<composebox::mojom::PageHandlerFactory> receiver) {
  if (!base::FeatureList::IsEnabled(omnibox::kOmniboxEverywhere)) {
    return;
  }
  if (composebox_page_factory_receiver_.is_bound()) {
    composebox_page_factory_receiver_.reset();
  }
  composebox_page_factory_receiver_.Bind(std::move(receiver));
}

void OmniboxEverywhereUI::CreatePageHandler(
    mojo::PendingReceiver<composebox::mojom::PageHandler> pending_page_handler,
    mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler>
        pending_searchbox_handler) {
  // TODO(crbug.com/526629960): Create new EverywhereComposeboxHandler or allow
  // the ComposeboxHandler to parameterize the OmniboxClient.
  composebox_handler_ = std::make_unique<ComposeboxEverywhereHandler>(
      std::move(pending_page_handler), std::move(pending_searchbox_handler),
      std::move(pending_searchbox_page), profile_, web_ui()->GetWebContents(),
      base::BindRepeating(
          &OmniboxEverywhereUI::GetOrCreateContextualSessionHandle,
          base::Unretained(this)),
      base::BindRepeating(&OmniboxEverywhereUI::ClearContextualSessionHandle,
                          base::Unretained(this)));
}

void OmniboxEverywhereUI::BindInterface(
    content::RenderFrameHost* host,
    mojo::PendingReceiver<searchbox::mojom::PageHandlerFactory>
        pending_page_handler) {
  if (searchbox_page_factory_receiver_.is_bound()) {
    searchbox_page_factory_receiver_.reset();
  }
  searchbox_page_factory_receiver_.Bind(std::move(pending_page_handler));
}

void OmniboxEverywhereUI::CreatePageHandler(
    mojo::PendingRemote<searchbox::mojom::Page> page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler) {
  auto* service = OmniboxEverywhereServiceFactory::GetForProfile(profile_);
  CHECK(service);

  MetricsReporterService* metrics_reporter_service =
      MetricsReporterService::GetFromWebContents(web_ui()->GetWebContents());
  omnibox_handler_ = std::make_unique<OmniboxEverywhereHandler>(
      std::move(pending_page_handler), std::move(page),
      metrics_reporter_service->metrics_reporter(), web_ui(), service,
      base::BindRepeating(
          &OmniboxEverywhereUI::GetOrCreateContextualSessionHandle,
          base::Unretained(this)));
}

contextual_search::ContextualSearchSessionHandle*
OmniboxEverywhereUI::GetOrCreateContextualSessionHandle() {
  if (!shared_session_handle_) {
    auto* contextual_search_service =
        ContextualSearchServiceFactory::GetForProfile(profile_);
    if (contextual_search_service) {
      shared_session_handle_ = contextual_search_service->CreateSession(
          omnibox::CreateQueryControllerConfigParams(),
          contextual_search::ContextualSearchSource::kOmniboxEverywhere,
          lens::LensOverlayInvocationSource::kOmniboxEverywhereComposebox);
      shared_session_handle_->CheckSearchContentSharingSettings(
          profile_->GetPrefs());
    }
  }
  return shared_session_handle_.get();
}

void OmniboxEverywhereUI::ClearContextualSessionHandle() {
  shared_session_handle_.reset();
}

WEB_UI_CONTROLLER_TYPE_IMPL(OmniboxEverywhereUI)
