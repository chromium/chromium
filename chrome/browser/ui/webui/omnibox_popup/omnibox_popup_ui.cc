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
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter_service.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_aim_handler.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_handler.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/sanitized_image/sanitized_image_source.h"
#include "chrome/browser/ui/webui/searchbox/omnibox_composebox_handler.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/omnibox_popup_resources.h"
#include "chrome/grit/omnibox_popup_resources_map.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/lens/lens_features.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
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

}  // namespace

bool OmniboxPopupUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return omnibox::IsAimPopupFeatureEnabled() ||
         omnibox::IsWebUIOmniboxFullPopupEnabled() ||
         omnibox::IsWebUIOmniboxPopupEnabled() ||
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
      Profile::FromWebUI(web_ui), chrome::kChromeUIOmniboxPopupHost);

  bool session_allows_drag_and_drop = false;
  if (auto* session_handle = GetOrCreateContextualSessionHandle()) {
    session_allows_drag_and_drop =
        session_handle->CheckSearchContentSharingSettings(profile_->GetPrefs());
  }

  source->AddLocalizedStrings(SearchboxHandler::GetWebUIDataSourceDict(
      Profile::FromWebUI(web_ui),
      {.enable_voice_search = true,
       .session_allows_drag_and_drop = session_allows_drag_and_drop}));

  source->AddBoolean("isTopChromeSearchbox", true);
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
  source->AddBoolean("addTabUploadDelayOnRecentTabChipClick",
                     omnibox::kAddTabUploadDelayOnRecentTabChipClick.Get());
  source->AddBoolean("composeboxShowRecentTabChip",
                     omnibox::kShowRecentTabChip.Get());
  source->AddBoolean("composeboxShowTypedSuggest",
                     omnibox::kShowComposeboxTypedSuggest.Get());
  source->AddBoolean("composeboxShowZps", omnibox::kShowComposeboxZps.Get());
  source->AddBoolean("composeboxSmartComposeEnabled",
                     omnibox::kShowSmartCompose.Get());
  source->AddBoolean("contextButtonHasBackground",
                     omnibox::kContextButtonHasBackground.Get());
  source->AddBoolean("hideClassicContextButton",
                     omnibox::kHideClassicContextButton.Get());
  source->AddBoolean("composeboxForkEnabled",
                     omnibox::kUseComposeboxFork.Get());
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
  source->AddBoolean("contextButtonShapeIsOblong",
                     omnibox::kContextButtonShapeIsOblong.Get());

  webui::SetupWebUIDataSource(source, kOmniboxPopupResources,
                              omnibox::IsWebUIOmniboxFullPopupEnabled()
                                  ? IDR_OMNIBOX_POPUP_OMNIBOX_POPUP_FULL_HTML
                                  : IDR_OMNIBOX_POPUP_OMNIBOX_POPUP_HTML);
  webui::EnableTrustedTypesCSP(source);

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
  omnibox_handler_ = std::make_unique<WebuiOmniboxHandler>(
      std::move(pending_page_handler), std::move(page),
      metrics_reporter_service->metrics_reporter(), omnibox_controller,
      web_ui(),
      base::BindRepeating(&OmniboxPopupUI::GetOrCreateContextualSessionHandle,
                          base::Unretained(this)));
}

void OmniboxPopupUI::BindInterface(
    mojo::PendingReceiver<omnibox_popup::mojom::PageHandlerFactory> receiver) {
  popup_page_factory_receiver_.reset();
  popup_page_factory_receiver_.Bind(std::move(receiver));
}

void OmniboxPopupUI::CreatePageHandler(
    mojo::PendingRemote<omnibox_popup::mojom::Page> page,
    mojo::PendingReceiver<omnibox_popup::mojom::PageHandler> receiver) {
  popup_handler_ = std::make_unique<OmniboxPopupHandler>(std::move(receiver),
                                                         std::move(page));
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
  if (composebox_page_factory_receiver_.is_bound()) {
    composebox_page_factory_receiver_.reset();
  }
  composebox_page_factory_receiver_.Bind(std::move(receiver));
}

void OmniboxPopupUI::CreatePageHandler(
    mojo::PendingRemote<composebox::mojom::Page> pending_page,
    mojo::PendingReceiver<composebox::mojom::PageHandler> pending_page_handler,
    mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler>
        pending_searchbox_handler) {
  DCHECK(pending_page.is_valid());

  composebox_handler_ = std::make_unique<OmniboxComposeboxHandler>(
      std::move(pending_page_handler), std::move(pending_page),
      std::move(pending_searchbox_handler), std::move(pending_searchbox_page),
      profile_, web_ui()->GetWebContents(),
      base::BindRepeating(&OmniboxPopupUI::GetOrCreateContextualSessionHandle,
                          base::Unretained(this)),
      base::BindRepeating(&OmniboxPopupUI::ClearContextualSessionHandle,
                          base::Unretained(this)));
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
