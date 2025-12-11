// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_query_flow_router.h"

#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/lens/core/mojom/lens.mojom.h"
#include "chrome/browser/ui/lens/lens_overlay_image_helper.h"
#include "chrome/browser/ui/lens/lens_search_contextualization_controller.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/lens/ref_counted_lens_overlay_client_logs.h"
#include "components/sessions/content/session_tab_helper.h"

namespace {
std::vector<lens::ContextualInput> ConvertPageContentToContextualInput(
    base::span<const lens::PageContent> underlying_page_contents) {
  std::vector<lens::ContextualInput> contextual_inputs;
  for (const auto& page_content : underlying_page_contents) {
    lens::ContextualInput contextual_input;
    contextual_input.bytes_ = page_content.bytes_;
    contextual_input.content_type_ = page_content.content_type_;
    contextual_inputs.push_back(contextual_input);
  }
  return contextual_inputs;
}
}  // namespace

namespace lens {

LensQueryFlowRouter::LensQueryFlowRouter(
    LensSearchController* lens_search_controller)
    : lens_search_controller_(lens_search_controller) {}

LensQueryFlowRouter::~LensQueryFlowRouter() = default;

std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
LensQueryFlowRouter::CreateContextualSearchSessionHandle() {
  auto* contextual_search_service =
      ContextualSearchServiceFactory::GetForProfile(profile());
  // TODO(crbug.com/463400248): Use contextual tasks config params for Lens
  // requests.
  return contextual_search_service->CreateSession(
      ntp_composebox::CreateQueryControllerConfigParams(),
      contextual_search::ContextualSearchSource::kLens);
}

bool LensQueryFlowRouter::IsOff() const {
  if (contextual_tasks::GetEnableLensInContextualTasks()) {
    // TODO(crbug.com/461909986): If the pending session handle is not present, then
    // the session handle can possibly already be bound to the contextual tasks
    // UI.
    return !pending_session_handle_;
  }
  return lens_overlay_query_controller()->IsOff();
}

void LensQueryFlowRouter::StartQueryFlow(
    const SkBitmap& screenshot,
    GURL page_url,
    std::optional<std::string> page_title,
    std::vector<lens::mojom::CenterRotatedBoxPtr> significant_region_boxes,
    base::span<const PageContent> underlying_page_contents,
    lens::MimeType primary_content_type,
    std::optional<uint32_t> pdf_current_page,
    float ui_scale_factor,
    base::TimeTicks invocation_time) {
  if (contextual_tasks::GetEnableLensInContextualTasks()) {
    // Create a contextual session for this WebContents if one does not exist.
    CHECK(!pending_session_handle_);
    pending_session_handle_ = CreateContextualSearchSessionHandle();
    pending_session_handle_->NotifySessionStarted();
    // Start uploading the current viewport and page content.
    UploadContextualInputData(
        pending_session_handle_.get(),
        CreateContextualInputData(screenshot, page_url, page_title,
                                  std::move(significant_region_boxes),
                                  underlying_page_contents,
                                  primary_content_type, pdf_current_page,
                                  ui_scale_factor, invocation_time));
    return;
  }

  lens_overlay_query_controller()->StartQueryFlow(
      screenshot, page_url, page_title, std::move(significant_region_boxes),
      underlying_page_contents, primary_content_type, pdf_current_page,
      ui_scale_factor, invocation_time);
}

void LensQueryFlowRouter::MaybeRestartQueryFlow() {
  lens_overlay_query_controller()->MaybeRestartQueryFlow();
}

void LensQueryFlowRouter::SendRegionSearch(
    base::Time query_start_time,
    lens::mojom::CenterRotatedBoxPtr region,
    lens::LensOverlaySelectionType lens_selection_type,
    std::map<std::string, std::string> additional_search_query_params,
    std::optional<SkBitmap> region_bytes) {
  if (contextual_tasks::GetEnableLensInContextualTasks()) {
    SendInteractionToContextualTasks(CreateSearchUrlRequestInfoFromInteraction(
        std::move(region), std::move(region_bytes), /*query_text=*/std::nullopt,
        lens_selection_type, additional_search_query_params, query_start_time));
    return;
  }

  lens_overlay_query_controller()->SendRegionSearch(
      query_start_time, std::move(region), lens_selection_type,
      additional_search_query_params, region_bytes);
}

void LensQueryFlowRouter::SendTextOnlyQuery(
    base::Time query_start_time,
    const std::string& query_text,
    lens::LensOverlaySelectionType lens_selection_type,
    std::map<std::string, std::string> additional_search_query_params) {
  if (contextual_tasks::GetEnableLensInContextualTasks()) {
    SendInteractionToContextualTasks(CreateSearchUrlRequestInfoFromInteraction(
        /*region=*/nullptr, /*region_bytes=*/std::nullopt, query_text,
        lens_selection_type, additional_search_query_params, query_start_time));
    return;
  }

  lens_overlay_query_controller()->SendTextOnlyQuery(
      query_start_time, query_text, lens_selection_type,
      additional_search_query_params);
}

void LensQueryFlowRouter::SendContextualTextQuery(
    base::Time query_start_time,
    const std::string& query_text,
    lens::LensOverlaySelectionType lens_selection_type,
    std::map<std::string, std::string> additional_search_query_params) {
  if (contextual_tasks::GetEnableLensInContextualTasks()) {
    SendInteractionToContextualTasks(CreateSearchUrlRequestInfoFromInteraction(
        /*region=*/nullptr, /*region_bytes=*/std::nullopt, query_text,
        lens_selection_type, additional_search_query_params, query_start_time));
    return;
  }

  lens_overlay_query_controller()->SendContextualTextQuery(
      query_start_time, query_text, lens_selection_type,
      additional_search_query_params);
}

void LensQueryFlowRouter::SendMultimodalRequest(
    base::Time query_start_time,
    lens::mojom::CenterRotatedBoxPtr region,
    const std::string& query_text,
    lens::LensOverlaySelectionType lens_selection_type,
    std::map<std::string, std::string> additional_search_query_params,
    std::optional<SkBitmap> region_bytes) {
  if (contextual_tasks::GetEnableLensInContextualTasks()) {
    SendInteractionToContextualTasks(CreateSearchUrlRequestInfoFromInteraction(
        std::move(region), std::move(region_bytes), query_text,
        lens_selection_type, additional_search_query_params, query_start_time));
    return;
  }

  lens_overlay_query_controller()->SendMultimodalRequest(
      query_start_time, std::move(region), query_text, lens_selection_type,
      additional_search_query_params, region_bytes);
}

void LensQueryFlowRouter::SendInteractionToContextualTasks(
    std::unique_ptr<CreateSearchUrlRequestInfo> request_info) {
  // TODO(crbug.com/461911257): Currently, follow up visual selections that are
  // made once the contextual tasks panel is already open / session handle has
  // been moved will do nothing. In the future, they should grab the current
  // open session handle and use that to send the interaction request.
  if (!pending_session_handle_) {
    return;
  }

  auto search_url =
      pending_session_handle_->CreateSearchUrl(std::move(request_info));
  OpenContextualTasksPanel(search_url);
}

void LensQueryFlowRouter::OpenContextualTasksPanel(const GURL& url) {
  // Show the side panel. This will create a new task and associate it with the
  // active tab.
  contextual_tasks::ContextualTasksUiServiceFactory::GetForBrowserContext(
      web_contents()->GetBrowserContext())
      ->StartTaskUiInSidePanel(browser_window_interface(), tab_interface(),
                               url);
}

void LensQueryFlowRouter::UploadContextualInputData(
    contextual_search::ContextualSearchSessionHandle* session_handle,
    std::unique_ptr<lens::ContextualInputData> contextual_input_data) {
  session_handle->AddTabContext(
      sessions::SessionTabHelper::IdForTab(web_contents()).id(),
      base::BindOnce(&LensQueryFlowRouter::OnFinishedAddingTabContext,
                     weak_factory_.GetWeakPtr(), session_handle,
                     std::move(contextual_input_data)));
}

void LensQueryFlowRouter::OnFinishedAddingTabContext(
    contextual_search::ContextualSearchSessionHandle* session_handle,
    std::unique_ptr<lens::ContextualInputData> contextual_input_data,
    const base::UnguessableToken& token) {
  // TODO(crbug.com/463400248): Use contextual tasks image upload config params
  // for Lens requests.
  auto image_upload_config =
      ntp_composebox::FeatureConfig::Get().config.composebox().image_upload();
  auto image_options = lens::ImageEncodingOptions{
      .enable_webp_encoding = image_upload_config.enable_webp_encoding(),
      .max_size = image_upload_config.downscale_max_image_size(),
      .max_height = image_upload_config.downscale_max_image_height(),
      .max_width = image_upload_config.downscale_max_image_width(),
      .compression_quality = image_upload_config.image_compression_quality()};

  session_handle->StartTabContextUploadFlow(
      token, std::move(contextual_input_data), std::move(image_options));
}

std::unique_ptr<lens::ContextualInputData>
LensQueryFlowRouter::CreateContextualInputData(
    const SkBitmap& screenshot,
    GURL page_url,
    std::optional<std::string> page_title,
    std::vector<lens::mojom::CenterRotatedBoxPtr> significant_region_boxes,
    base::span<const PageContent> underlying_page_contents,
    lens::MimeType primary_content_type,
    std::optional<uint32_t> pdf_current_page,
    float ui_scale_factor,
    base::TimeTicks invocation_time) {
  auto contextual_input_data = std::make_unique<lens::ContextualInputData>();
  contextual_input_data->context_input =
      ConvertPageContentToContextualInput(underlying_page_contents);
  contextual_input_data->page_url = page_url;
  contextual_input_data->page_title = page_title;
  contextual_input_data->primary_content_type = primary_content_type;
  contextual_input_data->pdf_current_page = pdf_current_page;
  contextual_input_data->viewport_screenshot = screenshot;
  contextual_input_data->is_page_context_eligible =
      lens_search_contextualization_controller()
          ->GetCurrentPageContextEligibility();
  contextual_input_data->tab_session_id =
      sessions::SessionTabHelper::IdForTab(web_contents());
  return contextual_input_data;
}

std::unique_ptr<CreateSearchUrlRequestInfo>
LensQueryFlowRouter::CreateSearchUrlRequestInfoFromInteraction(
    lens::mojom::CenterRotatedBoxPtr region,
    std::optional<SkBitmap> region_bytes,
    std::optional<std::string> query_text,
    lens::LensOverlaySelectionType lens_selection_type,
    std::map<std::string, std::string> additional_search_query_params,
    base::Time query_start_time) {
  auto request_info = std::make_unique<CreateSearchUrlRequestInfo>();
  request_info->search_url_type = SearchUrlType::kStandard;
  if (query_text.has_value()) {
    request_info->query_text = query_text.value();
  }
  request_info->query_start_time = query_start_time;
  request_info->lens_overlay_selection_type = lens_selection_type;
  request_info->additional_params = additional_search_query_params;

  if (region) {
    auto client_logs =
        base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
    auto image_crop_and_bitmap = lens::DownscaleAndEncodeBitmapRegionIfNeeded(
        lens_search_contextualization_controller()->viewport_screenshot(),
        region->Clone(), region_bytes, client_logs);
    if (image_crop_and_bitmap) {
      request_info->image_crop = std::move(image_crop_and_bitmap->image_crop);
    }
  }
  return request_info;
}

}  // namespace lens
