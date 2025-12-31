// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_query_flow_router.h"

#include "base/rand_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/lens/core/mojom/lens.mojom.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_gen204_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_image_helper.h"
#include "chrome/browser/ui/lens/lens_overlay_proto_converter.h"
#include "chrome/browser/ui/lens/lens_overlay_url_builder.h"
#include "chrome/browser/ui/lens/lens_search_contextualization_controller.h"
#include "chrome/browser/ui/lens/lens_search_feature_flag_utils.h"
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
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"

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
  if (ShouldRouteToContextualTasks()) {
    auto* session_handle = GetContextualSearchSessionHandle();
    if (session_handle && session_handle->GetController()) {
      session_handle->GetController()->RemoveObserver(this);
    }
    gen204_controller()->OnQueryFlowEnd();
  }
}

bool LensQueryFlowRouter::IsOff() const {
  if (ShouldRouteToContextualTasks()) {
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
  if (ShouldRouteToContextualTasks()) {
    CHECK(lens_search_controller_->invocation_source().has_value());
    gen204_id_ = base::RandUint64();
    gen204_controller()->OnQueryFlowStart(
        lens_search_controller_->invocation_source().value(), profile(),
        gen204_id_);

    // Create a contextual session for this WebContents if one does not exist.
    CHECK(!pending_session_handle_);
    pending_session_handle_ = CreateContextualSearchSessionHandle();
    pending_session_handle_->NotifySessionStarted();

    // Add observer to listen for file upload status changes.
    pending_session_handle_->GetController()->AddObserver(this);

    // If permissions have been granted, start uploading the current viewport
    // and page content. If not, store as a callback to be run later.
    auto upload_task =
        base::BindOnce(&LensQueryFlowRouter::UploadContextualInputData,
                       weak_factory_.GetWeakPtr(),
                       CreateContextualInputData(
                           screenshot, page_url, page_title,
                           std::move(significant_region_boxes),
                           underlying_page_contents, primary_content_type,
                           pdf_current_page, ui_scale_factor, invocation_time));

    if (lens::features::IsLensOverlayNonBlockingPrivacyNoticeEnabled() &&
        !lens::DidUserGrantLensOverlayNeededPermissions(
            profile()->GetPrefs())) {
      pending_upload_request_ = std::move(upload_task);
    } else {
      std::move(upload_task).Run();
    }
    return;
  }

  lens_overlay_query_controller()->StartQueryFlow(
      screenshot, page_url, page_title, std::move(significant_region_boxes),
      underlying_page_contents, primary_content_type, pdf_current_page,
      ui_scale_factor, invocation_time);
}

void LensQueryFlowRouter::MaybeResumeQueryFlow() {
  if (ShouldRouteToContextualTasks() && pending_upload_request_) {
    std::move(pending_upload_request_).Run();
  }
}

void LensQueryFlowRouter::MaybeRestartQueryFlow() {
  if (ShouldRouteToContextualTasks()) {
    return;
  }
  lens_overlay_query_controller()->MaybeRestartQueryFlow();
}

void LensQueryFlowRouter::SendTaskCompletionGen204IfEnabled(
    lens::mojom::UserAction user_action) {
  if (ShouldRouteToContextualTasks()) {
    auto* session_handle = GetContextualSearchSessionHandle();
    if (!session_handle || !session_handle->GetController() ||
        !overlay_tab_context_file_token_.has_value()) {
      return;
    }
    auto* file_info = session_handle->GetController()->GetFileInfo(
        overlay_tab_context_file_token_.value());
    if (!file_info) {
      return;
    }

    gen204_controller()->SendTaskCompletionGen204IfEnabled(
        /*encoded_analytics_id=*/file_info->request_id.analytics_id(),
        user_action,
        /*request_id=*/file_info->request_id);
    return;
  }
  lens_overlay_query_controller()->SendTaskCompletionGen204IfEnabled(
      user_action);
}

void LensQueryFlowRouter::SendSemanticEventGen204IfEnabled(
    lens::mojom::SemanticEvent event) {
  if (ShouldRouteToContextualTasks()) {
    auto* session_handle = GetContextualSearchSessionHandle();
    if (!session_handle || !session_handle->GetController() ||
        !overlay_tab_context_file_token_.has_value()) {
      return;
    }
    auto* file_info = session_handle->GetController()->GetFileInfo(
        overlay_tab_context_file_token_.value());
    if (!file_info) {
      return;
    }

    gen204_controller()->SendSemanticEventGen204IfEnabled(
        event, /*request_id=*/file_info->request_id);
    return;
  }
  lens_overlay_query_controller()->SendSemanticEventGen204IfEnabled(event);
}

std::optional<lens::proto::LensOverlaySuggestInputs>
LensQueryFlowRouter::GetSuggestInputs() {
  if (IsOff()) {
    return std::nullopt;
  }

  if (ShouldRouteToContextualTasks()) {
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
  // Return the callback immediately if the suggest inputs are already ready.
  if (AreLensSuggestInputsReady(GetSuggestInputs())) {
    callback.Run();
    return;
  }

  if (ShouldRouteToContextualTasks()) {
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
    std::optional<SkBitmap> region_bytes,
    lens::LensOverlayInvocationSource invocation_source) {
  if (ShouldRouteToContextualTasks()) {
    SendInteractionToContextualTasks(CreateSearchUrlRequestInfoFromInteraction(
        std::move(region), std::move(region_bytes), /*query_text=*/std::nullopt,
        lens_selection_type, additional_search_query_params, query_start_time,
        invocation_source));
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
    std::map<std::string, std::string> additional_search_query_params,
    lens::LensOverlayInvocationSource invocation_source) {
  if (ShouldRouteToContextualTasks()) {
    SendInteractionToContextualTasks(CreateSearchUrlRequestInfoFromInteraction(
        /*region=*/nullptr, /*region_bytes=*/std::nullopt, query_text,
        lens_selection_type, additional_search_query_params, query_start_time,
        invocation_source));
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
    std::map<std::string, std::string> additional_search_query_params,
    lens::LensOverlayInvocationSource invocation_source) {
  if (ShouldRouteToContextualTasks()) {
    auto request_info = CreateSearchUrlRequestInfoFromInteraction(
        /*region=*/nullptr, /*region_bytes=*/std::nullopt, query_text,
        lens_selection_type, additional_search_query_params, query_start_time,
        invocation_source);
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
    std::optional<SkBitmap> region_bytes,
    lens::LensOverlayInvocationSource invocation_source) {
  if (ShouldRouteToContextualTasks()) {
    SendInteractionToContextualTasks(CreateSearchUrlRequestInfoFromInteraction(
        std::move(region), std::move(region_bytes), query_text,
        lens_selection_type, additional_search_query_params, query_start_time,
        invocation_source));
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
  // TODO(crbug.com/469875837): Determine what to do with the return value
  // of this call, or move this call to a different location.
  session_handle->CheckSearchContentSharingSettings(profile()->GetPrefs());
  return session_handle;
}

const SkBitmap& LensQueryFlowRouter::GetViewportScreenshot() const {
  return lens_overlay_controller()->initial_screenshot();
}

void LensQueryFlowRouter::OnFileUploadStatusChangedForTesting(
    const base::UnguessableToken& file_token,
    lens::MimeType mime_type,
    contextual_search::FileUploadStatus file_upload_status,
    const std::optional<contextual_search::FileUploadErrorType>& error_type) {
  OnFileUploadStatusChanged(file_token, mime_type, file_upload_status,
                            error_type);
}

void LensQueryFlowRouter::HandleInteractionResponse(
    std::optional<lens::ImageCrop> image_crop,
    lens::LensOverlayInteractionResponse interaction_response) {
  if (interaction_response.has_text()) {
    lens::ZoomedCrop zoomed_crop;
    if (image_crop.has_value()) {
      zoomed_crop.CopyFrom(image_crop->zoomed_crop());
    }
    // TODO(crbug.com/469836056): Verify that this is the correct resized bitmap
    // to be using to generate the text once the interaction response returns
    // text.
    lens_search_controller_->HandleInteractionResponse(
        lens::CreateTextMojomFromInteractionResponse(
            interaction_response, zoomed_crop,
            gfx::Size(GetViewportScreenshot().width(),
                      GetViewportScreenshot().height())));
  }
}

void LensQueryFlowRouter::OnFileUploadStatusChanged(
    const base::UnguessableToken& file_token,
    lens::MimeType mime_type,
    contextual_search::FileUploadStatus file_upload_status,
    const std::optional<contextual_search::FileUploadErrorType>& error_type) {
  const auto& suggest_inputs = GetSuggestInputs();
  if (suggest_inputs.has_value() &&
      AreLensSuggestInputsReady(*suggest_inputs)) {
    if (suggest_inputs_ready_callback_) {
      suggest_inputs_ready_callback_.Run();
    }
  }

  auto* session_handle = GetContextualSearchSessionHandle();
  if (session_handle && overlay_tab_context_file_token_.has_value() &&
      file_token == overlay_tab_context_file_token_.value() &&
      file_upload_status ==
          contextual_search::FileUploadStatus::kUploadSuccessful) {
    // Pass any text that was returned as part of the file upload response to
    // to the overlay.
    auto* file_info = session_handle->GetController()->GetFileInfo(file_token);
    std::vector<lens::mojom::OverlayObjectPtr> objects;
    lens::mojom::TextPtr text = nullptr;
    if (file_info) {
      for (const auto& response_body : file_info->response_bodies) {
        lens::LensOverlayServerResponse server_response;
        if (server_response.ParseFromString(response_body) &&
            server_response.has_objects_response()) {
          text = lens::CreateTextMojomFromServerResponse(
              server_response, gfx::Size(GetViewportScreenshot().width(),
                                         GetViewportScreenshot().height()));
          objects =
              lens::CreateObjectsMojomArrayFromServerResponse(server_response);
        }
      }
    }
    lens_overlay_controller()->HandleStartQueryResponse(
        std::move(objects), std::move(text), /*is_error=*/false);
  }
}

void LensQueryFlowRouter::SendInteractionToContextualTasks(
    std::unique_ptr<CreateSearchUrlRequestInfo> request_info) {
  // If there is no existing session handle, then the search URL request will
  // need to wait for the tab context to be added before being sent.
  if (!GetContextualSearchSessionHandle()) {
    pending_session_handle_ = CreateContextualSearchSessionHandle();
    pending_session_handle_->NotifySessionStarted();
  }

  if (!overlay_tab_context_file_token_.has_value()) {
    pending_search_url_request_ = std::move(request_info);
    // Upload the page context when creating a session handle.
    if (auto* controller =
            TabContextualizationController::From(tab_interface())) {
      controller->GetPageContext(
          base::BindOnce(&LensQueryFlowRouter::UploadContextualInputData,
                         weak_factory_.GetWeakPtr()));
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
  // Notify the overlay controller that the side panel was opened so it can
  // update its UI state.
  lens_overlay_controller()->NotifyResultsPanelOpened();
}

void LensQueryFlowRouter::UploadContextualInputData(
    std::unique_ptr<lens::ContextualInputData> contextual_input_data) {
  auto* session_handle = GetContextualSearchSessionHandle();
  GetContextualSearchSessionHandle()->AddTabContext(
      sessions::SessionTabHelper::IdForTab(web_contents()).id(),
      base::BindOnce(&LensQueryFlowRouter::OnFinishedAddingTabContext,
                     weak_factory_.GetWeakPtr(), session_handle,
                     std::move(contextual_input_data)));
}

void LensQueryFlowRouter::OnFinishedAddingTabContext(
    contextual_search::ContextualSearchSessionHandle* session_handle,
    std::unique_ptr<lens::ContextualInputData> contextual_input_data,
    const base::UnguessableToken& token) {
  overlay_tab_context_file_token_ = token;
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
    // Add the tab context file token to the request's file tokens. This could
    // not be added earlier because the token is not known until this point.
    pending_search_url_request_->file_tokens.push_back(token);
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
    base::Time query_start_time,
    lens::LensOverlayInvocationSource invocation_source) {
  auto request_info = std::make_unique<CreateSearchUrlRequestInfo>();
  request_info->search_url_type = SearchUrlType::kStandard;
  if (query_text.has_value()) {
    request_info->query_text = query_text.value();
  }
  request_info->query_start_time = query_start_time;
  request_info->lens_overlay_selection_type = lens_selection_type;
  if (overlay_tab_context_file_token_.has_value()) {
    request_info->file_tokens.push_back(
        overlay_tab_context_file_token_.value());
  }

  // Add mandatory Lens specific query parameters if not already present.
  const bool has_text = query_text.has_value() && !query_text->empty();
  const bool has_image = region || region_bytes.has_value();
  lens::AppendLensOverlaySidePanelParams(additional_search_query_params,
                                         gen204_id_, has_text, has_image);

  request_info->additional_params = additional_search_query_params;
  request_info->invocation_source = invocation_source;

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
  request_info->interaction_response_callback =
      base::BindOnce(&LensQueryFlowRouter::HandleInteractionResponse,
                     weak_factory_.GetWeakPtr(), request_info->image_crop);
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
  if (!coordinator || !coordinator->IsSidePanelOpenForContextualTask()) {
    return nullptr;
  }

  return coordinator->GetContextualSearchSessionHandleForSidePanel();
}

}  // namespace lens
