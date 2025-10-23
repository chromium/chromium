// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"

#include <atomic>
#include <string_view>

#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter_service.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/composebox_handler.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/sanitized_image_source.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/omnibox_popup_resources.h"
#include "chrome/grit/omnibox_popup_resources_map.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/omnibox/composebox/composebox_metrics_recorder.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/webui_util.h"

bool OmniboxPopupUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxAimPopup) ||
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
  source->AddBoolean("composeboxShowContextMenu",
                     ntp_composebox::kShowContextMenu.Get());
  source->AddBoolean("composeboxShowContextMenuTabPreviews",
                     ntp_composebox::kShowContextMenuTabPreviews.Get());
  source->AddBoolean("composeboxShowZps",
                     ntp_composebox::kShowComposeboxZps.Get());
  source->AddBoolean("composeboxShowTypedSuggest",
                     ntp_composebox::kShowComposeboxTypedSuggest.Get());
  source->AddBoolean("composeboxShowImageSuggest",
                     ntp_composebox::kShowComposeboxImageSuggestions.Get());
  source->AddBoolean("composeboxShowContextMenuDescription",
                     ntp_composebox::kShowContextMenuDescription.Get());
  source->AddBoolean("composeboxShowSubmit", ntp_composebox::kShowSubmit.Get());
  source->AddBoolean("composeboxShowCreateImageButton", false);
  source->AddBoolean("composeboxShowDeepSearchButton", false);
  source->AddBoolean("composeboxShowPdfUpload", false);
  source->AddBoolean("composeboxShowRecentTabChip", false);
  source->AddBoolean("composeboxSmartComposeEnabled", false);

  auto composebox_config =
      ntp_composebox::FeatureConfig::Get().config.composebox();
  const std::string image_mime_types =
      composebox_config.image_upload().mime_types_allowed();
  source->AddString("composeboxImageFileTypes", image_mime_types);
  const std::string attachment_mime_types =
      composebox_config.attachment_upload().mime_types_allowed();
  source->AddString("composeboxAttachmentFileTypes", attachment_mime_types);
  source->AddInteger("composeboxFileMaxSize",
                     composebox_config.attachment_upload().max_size_bytes());
  source->AddInteger("composeboxFileMaxCount",
                     composebox_config.max_num_files());
  source->AddBoolean("composeboxCloseByEscape",
                     composebox_config.close_by_escape());
  source->AddBoolean("composeboxCloseByClickOutside",
                     composebox_config.close_by_click_outside());

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
  handler_ = std::make_unique<WebuiOmniboxHandler>(
      std::move(pending_page_handler), Profile::FromWebUI(web_ui()),
      web_ui()->GetWebContents(), metrics_reporter_service->metrics_reporter(),
      omnibox_controller);
}

void OmniboxPopupUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
        pending_receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(pending_receiver));
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

  constexpr char kComposeboxMetricsReporterPrefName[] = "NewTabPage.";

  composebox_handler_ = std::make_unique<ComposeboxHandler>(
      std::move(pending_page_handler), std::move(pending_page),
      std::move(pending_searchbox_handler),
      std::make_unique<ComposeboxMetricsRecorder>(
          kComposeboxMetricsReporterPrefName),
      profile_, web_ui()->GetWebContents());

  // TODO(crbug.com/435288212): Move searchbox mojom to use factory pattern.
  composebox_handler_->SetPage(std::move(pending_searchbox_page));
}
