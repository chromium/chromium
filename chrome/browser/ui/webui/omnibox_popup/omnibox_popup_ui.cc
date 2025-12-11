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
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter_service.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_aim_handler.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/sanitized_image_source.h"
#include "chrome/browser/ui/webui/searchbox/omnibox_composebox_handler.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/omnibox_popup_resources.h"
#include "chrome/grit/omnibox_popup_resources_map.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

namespace {

using AddContextButtonVariant = omnibox::AddContextButtonVariant;

std::string AddContextButtonVariantToSearchboxLayoutMode(
    AddContextButtonVariant variant) {
  switch (variant) {
    case AddContextButtonVariant::kNone:
      return "";
    case AddContextButtonVariant::kBelowResults:
      return "TallBottomContext";
    case AddContextButtonVariant::kAboveResults:
      return "TallTopContext";
    case AddContextButtonVariant::kInline:
      return "Compact";
  }

  return "";
}

}  // namespace

bool OmniboxPopupUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return omnibox::IsAimPopupFeatureEnabled() ||
         base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxFullPopup) ||
         base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxPopup);
}

OmniboxPopupUI::OmniboxPopupUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui,
                               true /* Needed for webui browser tests */),
      profile_(Profile::FromWebUI(web_ui)) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIOmniboxPopupHost);

  SearchboxHandler::SetupWebUIDataSource(source, Profile::FromWebUI(web_ui));

  source->AddBoolean("isTopChromeSearchbox", true);
  source->AddBoolean(
      "omniboxPopupDebugEnabled",
      base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxPopupDebug));

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
  source->AddInteger("composeboxFileMaxCount",
                     composebox_config.max_num_files());
  source->AddInteger("composeboxFileMaxSize",
                     composebox_config.attachment_upload().max_size_bytes());
  const std::string image_mime_types =
      composebox_config.image_upload().mime_types_allowed();
  source->AddString("composeboxImageFileTypes", image_mime_types);
  const auto* aim_eligibility_service =
      AimEligibilityServiceFactory::GetForProfile(profile_);
  bool show_pdf_upload = aim_eligibility_service &&
                         aim_eligibility_service->IsPdfUploadEligible() &&
                         composebox_config.is_pdf_upload_enabled();
  source->AddBoolean("composeboxShowPdfUpload", show_pdf_upload);

  source->AddBoolean(
      "showContextMenuEntrypoint",
      aim_eligibility_service && aim_eligibility_service->IsAimEligible());

  source->AddBoolean("composeboxCloseByClickOutside",
                     omnibox::kCloseComposeboxByClickOutside.Get());
  source->AddBoolean("composeboxCloseByEscape",
                     omnibox::kCloseComposeboxByEscape.Get());
  source->AddBoolean("composeboxContextMenuEnableMultiTabSelection",
                     omnibox::kContextMenuEnableMultiTabSelection.Get());
  source->AddBoolean("composeboxContextDragAndDropEnabled", false);
  source->AddBoolean("composeboxNoFlickerSuggestionsFix", false);
  source->AddBoolean("composeboxShowContextMenu",
                     omnibox::kShowContextMenu.Get());
  source->AddBoolean("composeboxShowContextMenuDescription",
                     omnibox::kShowContextMenuDescription.Get());
  source->AddBoolean("composeboxShowContextMenuTabPreviews",
                     omnibox::kShowContextMenuTabPreviews.Get());
  source->AddBoolean("composeboxShowCreateImageButton",
                     omnibox::IsCreateImagesEnabled(profile_));
  source->AddBoolean("composeboxShowDeepSearchButton",
                     omnibox::IsDeepSearchEnabled(profile_));
  source->AddBoolean("composeboxShowImageSuggest",
                     omnibox::kShowComposeboxImageSuggestions.Get());
  source->AddBoolean("composeboxShowLensSearchChip",
                     omnibox::kShowLensSearchChip.Get());
  source->AddBoolean("addTabUploadDelayOnRecentTabChipClick",
                     omnibox::kAddTabUploadDelayOnRecentTabChipClick.Get());
  source->AddBoolean("composeboxShowRecentTabChip",
                     omnibox::kShowRecentTabChip.Get());
  source->AddBoolean("composeboxShowSubmit", omnibox::kShowSubmit.Get());
  source->AddBoolean("composeboxShowTypedSuggestWithContext", false);
  source->AddBoolean("composeboxShowTypedSuggest",
                     omnibox::kShowComposeboxTypedSuggest.Get());
  source->AddBoolean("composeboxShowZps", omnibox::kShowComposeboxZps.Get());
  source->AddBoolean("composeboxSmartComposeEnabled",
                     omnibox::kShowSmartCompose.Get());
  source->AddBoolean("expandedComposeboxShowVoiceSearch",
                     omnibox::kShowVoiceSearchInExpandedComposebox.Get());
  source->AddBoolean("expandedSearchboxShowVoiceSearch",
                     false);
  const std::string searchbox_layout_mode =
      AddContextButtonVariantToSearchboxLayoutMode(
          omnibox::kWebUIOmniboxAimPopupAddContextButtonVariantParam.Get());
  source->AddString("searchboxLayoutMode", searchbox_layout_mode);
  source->AddBoolean("steadyComposeboxShowVoiceSearch", omnibox::kShowVoiceSearchInSteadyComposebox.Get());
  source->AddString(
      "composeboxSource",
      contextual_search::ContextualSearchMetricsRecorder::
          ContextualSearchSourceToString(
              contextual_search::ContextualSearchSource::kOmnibox));

  webui::SetupWebUIDataSource(
      source, kOmniboxPopupResources,
      base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxFullPopup)
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
    mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler) {
  auto* omnibox_controller =
      OmniboxPopupWebContentsHelper::GetOrCreateForWebContents(
          web_ui()->GetWebContents())
          ->get_omnibox_controller();
  CHECK(omnibox_controller);

  MetricsReporterService* metrics_reporter_service =
      MetricsReporterService::GetFromWebContents(web_ui()->GetWebContents());
  omnibox_handler_ = std::make_unique<WebuiOmniboxHandler>(
      std::move(pending_page_handler),
      metrics_reporter_service->metrics_reporter(), omnibox_controller,
      web_ui());
  omnibox_handler_->SetEmbedder(embedder());
}

void OmniboxPopupUI::BindInterface(
    mojo::PendingReceiver<composebox::mojom::PageHandlerFactory> receiver) {
  if (composebox_page_factory_receiver_.is_bound()) {
    composebox_page_factory_receiver_.reset();
  }
  composebox_page_factory_receiver_.Bind(std::move(receiver));
}

void OmniboxPopupUI::BindInterface(
    mojo::PendingReceiver<omnibox_popup_aim::mojom::PageHandlerFactory>
        receiver) {
  aim_page_factory_receiver_.reset();
  aim_page_factory_receiver_.Bind(std::move(receiver));
}

void OmniboxPopupUI::CreatePageHandler(
    mojo::PendingRemote<composebox::mojom::Page> pending_page,
    mojo::PendingReceiver<composebox::mojom::PageHandler> pending_page_handler,
    mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler>
        pending_searchbox_handler) {
  DCHECK(pending_page.is_valid());

  // Create a contextual session for this WebContents if one does not exist.
  if (auto* contextual_search_web_contents_helper =
          ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
              web_ui()->GetWebContents());
      !contextual_search_web_contents_helper->session_handle()) {
    auto* contextual_search_service =
        ContextualSearchServiceFactory::GetForProfile(profile_);
    auto contextual_session_handle = contextual_search_service->CreateSession(
        omnibox::CreateQueryControllerConfigParams(),
        contextual_search::ContextualSearchSource::kOmnibox);
    contextual_search_web_contents_helper->set_session_handle(
        std::move(contextual_session_handle));

    composebox_handler_ = std::make_unique<OmniboxComposeboxHandler>(
        std::move(pending_page_handler), std::move(pending_page),
        std::move(pending_searchbox_handler), profile_,
        web_ui()->GetWebContents());

    // TODO(crbug.com/435288212): Move searchbox mojom to use factory pattern.
    composebox_handler_->SetPage(std::move(pending_searchbox_page));
    composebox_handler_->SetEmbedder(embedder());
  }
}

void OmniboxPopupUI::CreatePageHandler(
    mojo::PendingRemote<omnibox_popup_aim::mojom::Page> page,
    mojo::PendingReceiver<omnibox_popup_aim::mojom::PageHandler> receiver) {
  popup_aim_handler_ = std::make_unique<OmniboxPopupAimHandler>(
      std::move(receiver), std::move(page), this);
}
