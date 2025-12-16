// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_query_flow_router.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/lens/core/mojom/lens.mojom.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_image_helper.h"
#include "chrome/browser/ui/lens/lens_search_contextualization_controller.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/lens/lens_url_utils.h"
#include "components/lens/ref_counted_lens_overlay_client_logs.h"
#include "components/sessions/content/session_tab_helper.h"
#include "net/base/url_util.h"

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

LensQueryFlowRouter::~LensQueryFlowRouter() {
  if (contextual_tasks::GetEnableLensInContextualTasks()) {
    auto* session_handle = GetContextualSearchSessionHandle();
    if (session_handle && session_handle->GetController()) {
      session_handle->GetController()->RemoveObserver(this);
    }
  }
}

bool LensQueryFlowRouter::IsOff() const {
  if (contextual_tasks::GetEnableLensInContextualTasks()) {
    return !GetContextualSearchSessionHandle();
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

    // Add observer to listen for file upload status changes.
    pending_session_handle_->GetController()->AddObserver(this);

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

std::optional<lens::proto::LensOverlaySuggestInputs>
LensQueryFlowRouter::GetSuggestInputs() {
  if (IsOff()) {
    return std::nullopt;
  }

  if (contextual_tasks::GetEnableLensInContextualTasks()) {
    auto* session_handle = GetContextualSearchSessionHandle();
    if (session_handle) {
      return session_handle->GetSuggestInputs();
    }
    return std::nullopt;
  }

  return std::make_optional(
      lens_overlay_query_controller()->GetLensSuggestInputs());
}

void LensQueryFlowRouter::SetSuggestInputsReadyCallback(
    base::RepeatingClosure callback) {
  // If Lens in contextual tasks is enabled, setup a callback to be called when
  // the file upload status changes, which can then be used to know if the
  // suggest inputs are ready.
  if (IsOff()) {
    return;
  }

  // Return the callback immediately if the suggest inputs are already ready.
  if (AreLensSuggestInputsReady(GetSuggestInputs())) {
    callback.Run();
  }

  if (contextual_tasks::GetEnableLensInContextualTasks()) {
    suggest_inputs_ready_callback_ = std::move(callback);

    // If the session handle doesn't exist yet, the observer will be added
    // once it is created.
    auto* session_handle = GetContextualSearchSessionHandle();
    if (session_handle && session_handle->GetController()) {
      session_handle->GetController()->AddObserver(this);
    }
    return;
  }
  lens_overlay_query_controller()->SetSuggestInputsReadyCallback(
      std::move(callback));
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
    auto request_info = CreateSearchUrlRequestInfoFromInteraction(
        /*region=*/nullptr, /*region_bytes=*/std::nullopt, query_text,
        lens_selection_type, additional_search_query_params, query_start_time);
    request_info->search_url_type = SearchUrlType::kAim;
    SendInteractionToContextualTasks(std::move(request_info));
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

std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
LensQueryFlowRouter::CreateContextualSearchSessionHandle() {
  auto* contextual_search_service =
      ContextualSearchServiceFactory::GetForProfile(profile());
  // TODO(crbug.com/463400248): Use contextual tasks config params for Lens
  // requests.
  auto session_handle = contextual_search_service->CreateSession(
      ntp_composebox::CreateQueryControllerConfigParams(),
      contextual_search::ContextualSearchSource::kLens);
  return session_handle;
}

const SkBitmap& LensQueryFlowRouter::GetViewportScreenshot() const {
  return lens_search_controller_->lens_overlay_controller()
      ->initial_screenshot();
}

void LensQueryFlowRouter::OnFileUploadStatusChanged(
    const base::UnguessableToken& file_token,
    lens::MimeType mime_type,
    contextual_search::FileUploadStatus file_upload_status,
    const std::optional<contextual_search::FileUploadErrorType>& error_type) {
  const auto& suggest_inputs = GetSuggestInputs();
  if (!suggest_inputs.has_value()) {
    return;
  }
  if (AreLensSuggestInputsReady(GetSuggestInputs())) {
    if (suggest_inputs_ready_callback_) {
      suggest_inputs_ready_callback_.Run();
    }

    auto* session_handle = GetContextualSearchSessionHandle();
    if (session_handle && session_handle->GetController()) {
      session_handle->GetController()->RemoveObserver(this);
    }
  }
}

void LensQueryFlowRouter::SendInteractionToContextualTasks(
    std::unique_ptr<CreateSearchUrlRequestInfo> request_info) {
  // If there is no existing session handle, then the search URL request will
  // need to wait for the tab context to be added before being sent.
  if (!GetContextualSearchSessionHandle()) {
    pending_session_handle_ = CreateContextualSearchSessionHandle();
    pending_session_handle_->NotifySessionStarted();
    pending_search_url_request_ = std::move(request_info);
    // Upload the page context when creating a session handle.
    if (auto* controller =
            TabContextualizationController::From(tab_interface())) {
      controller->GetPageContext(base::BindOnce(
          &LensQueryFlowRouter::UploadContextualInputData,
          weak_factory_.GetWeakPtr(), pending_session_handle_.get()));
    }
    return;
  }
  GetContextualSearchSessionHandle()->CreateSearchUrl(
      std::move(request_info),
      base::BindOnce(&LensQueryFlowRouter::OpenContextualTasksPanel,
                     weak_factory_.GetWeakPtr()));
}

void LensQueryFlowRouter::OpenContextualTasksPanel(GURL url) {
  // Show the side panel. This will create a new task and associate it with the
  // active tab.
  contextual_tasks::ContextualTasksUiServiceFactory::GetForBrowserContext(
      web_contents()->GetBrowserContext())
      ->StartTaskUiInSidePanel(browser_window_interface(), tab_interface(), url,
                               std::move(pending_session_handle_));
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

  // While a pending search URL request does not need to wait for the tab
  // context to upload, it does need to wait for tab context to be added to the
  // session handle before creating the search URL so it is properly
  // contextualized.
  if (pending_search_url_request_) {
    session_handle->CreateSearchUrl(
        std::move(pending_search_url_request_),
        base::BindOnce(&LensQueryFlowRouter::OpenContextualTasksPanel,
                       weak_factory_.GetWeakPtr()));
  }
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
        GetViewportScreenshot(), region->Clone(), region_bytes,
        std::move(client_logs));
    if (image_crop_and_bitmap) {
      request_info->image_crop = std::move(image_crop_and_bitmap->image_crop);
    }
  }
  return request_info;
}

contextual_search::ContextualSearchSessionHandle*
LensQueryFlowRouter::GetContextualSearchSessionHandle() const {
  if (pending_session_handle_) {
    return pending_session_handle_.get();
  }

  auto* coordinator =
      contextual_tasks::ContextualTasksSidePanelCoordinator::From(
          browser_window_interface());
  if (!coordinator) {
    return nullptr;
  }

  auto* web_contents = coordinator->GetActiveWebContents();
  if (!web_contents || !coordinator->IsSidePanelOpenForContextualTask()) {
    return nullptr;
  }

  auto* helper =
      ContextualSearchWebContentsHelper::FromWebContents(web_contents);
  if (helper && helper->session_handle()) {
    return helper->session_handle();
  }
  return nullptr;
}

}  // namespace lens
