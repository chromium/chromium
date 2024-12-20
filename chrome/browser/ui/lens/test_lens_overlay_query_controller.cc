// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test_lens_overlay_query_controller.h"

#include "google_apis/common/api_error_codes.h"

namespace lens {

FakeEndpointFetcher::FakeEndpointFetcher(EndpointResponse response)
    : EndpointFetcher(
          net::DefineNetworkTrafficAnnotation("lens_overlay_mock_fetcher",
                                              R"()")),
      response_(response) {}

void FakeEndpointFetcher::PerformRequest(
    EndpointFetcherCallback endpoint_fetcher_callback,
    const char* key) {
  if (!disable_responding_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(endpoint_fetcher_callback),
                       std::make_unique<EndpointResponse>(response_)));
  }
}

TestLensOverlayQueryController::TestLensOverlayQueryController(
    LensOverlayFullImageResponseCallback full_image_callback,
    LensOverlayUrlResponseCallback url_callback,
    LensOverlaySuggestInputsCallback interaction_data_callback,
    LensOverlayThumbnailCreatedCallback thumbnail_created_callback,
    variations::VariationsClient* variations_client,
    signin::IdentityManager* identity_manager,
    Profile* profile,
    lens::LensOverlayInvocationSource invocation_source,
    bool use_dark_mode,
    lens::LensOverlayGen204Controller* gen204_controller)
    : LensOverlayQueryController(full_image_callback,
                                 url_callback,
                                 interaction_data_callback,
                                 thumbnail_created_callback,
                                 variations_client,
                                 identity_manager,
                                 profile,
                                 invocation_source,
                                 use_dark_mode,
                                 gen204_controller) {}

TestLensOverlayQueryController::~TestLensOverlayQueryController() = default;

void TestLensOverlayQueryController::StartQueryFlow(
    const SkBitmap& screenshot,
    GURL page_url,
    std::optional<std::string> page_title,
    std::vector<lens::mojom::CenterRotatedBoxPtr> significant_region_boxes,
    base::span<const uint8_t> underlying_content_bytes,
    lens::MimeType underlying_content_type,
    float ui_scale_factor,
    base::TimeTicks invocation_time) {
  last_sent_underlying_content_bytes_ = underlying_content_bytes;
  last_sent_underlying_content_type_ = underlying_content_type;
  last_sent_page_url_ = page_url;

  LensOverlayQueryController::StartQueryFlow(
      screenshot, page_url, page_title, std::move(significant_region_boxes),
      underlying_content_bytes, underlying_content_type, ui_scale_factor,
      invocation_time);
}

void TestLensOverlayQueryController::SendTaskCompletionGen204IfEnabled(
    lens::mojom::UserAction user_action) {
  last_user_action_ = user_action;
}

void TestLensOverlayQueryController::SendRegionSearch(
    lens::mojom::CenterRotatedBoxPtr region,
    lens::LensOverlaySelectionType selection_type,
    std::map<std::string, std::string> additional_search_query_params,
    std::optional<SkBitmap> region_bytes) {
  last_queried_region_ = region->Clone();
  last_queried_region_bytes_ = region_bytes;
  last_lens_selection_type_ = selection_type;

  LensOverlayQueryController::SendRegionSearch(
      std::move(region), selection_type, additional_search_query_params,
      region_bytes);
}

void TestLensOverlayQueryController::SendTextOnlyQuery(
    const std::string& query_text,
    lens::LensOverlaySelectionType lens_selection_type,
    std::map<std::string, std::string> additional_search_query_params) {
  last_queried_text_ = query_text;
  last_lens_selection_type_ = lens_selection_type;

  LensOverlayQueryController::SendTextOnlyQuery(query_text, lens_selection_type,
                                                additional_search_query_params);
}

void TestLensOverlayQueryController::SendMultimodalRequest(
    lens::mojom::CenterRotatedBoxPtr region,
    const std::string& query_text,
    lens::LensOverlaySelectionType multimodal_selection_type,
    std::map<std::string, std::string> additional_search_query_params,
    std::optional<SkBitmap> region_bitmap) {
  last_queried_region_ = region.Clone();
  last_queried_region_bytes_ = region_bitmap;
  last_queried_text_ = query_text;
  last_lens_selection_type_ = multimodal_selection_type;

  LensOverlayQueryController::SendMultimodalRequest(
      std::move(region), query_text, multimodal_selection_type,
      additional_search_query_params, region_bitmap);
}

void TestLensOverlayQueryController::SendContextualTextQuery(
    const std::string& query_text,
    lens::LensOverlaySelectionType lens_selection_type,
    std::map<std::string, std::string> additional_search_query_params) {
  last_queried_text_ = query_text;
  last_lens_selection_type_ = lens_selection_type;

  LensOverlayQueryController::SendContextualTextQuery(
      query_text, lens_selection_type, additional_search_query_params);
}

void TestLensOverlayQueryController::SendPageContentUpdateRequest(
    base::span<const uint8_t> new_content_bytes,
    lens::MimeType new_content_type,
    GURL new_page_url) {
  // TODO(crbug.com/378918804): Update these variables in the endpoint
  // fetcher creator.
  last_sent_underlying_content_bytes_ = new_content_bytes;
  last_sent_underlying_content_type_ = new_content_type;
  last_sent_page_url_ = new_page_url;

  LensOverlayQueryController::SendPageContentUpdateRequest(
      new_content_bytes, new_content_type, new_page_url);
}

void TestLensOverlayQueryController::SendPartialPageContentRequest(
    base::span<const std::u16string> partial_content) {
  last_sent_partial_content_ = partial_content;
  LensOverlayQueryController::SendPartialPageContentRequest(partial_content);
}

void TestLensOverlayQueryController::ResetTestingState() {
  last_lens_selection_type_ = lens::UNKNOWN_SELECTION_TYPE;
  last_queried_region_.reset();
  last_queried_text_.clear();
  last_queried_region_bytes_ = std::nullopt;
  last_sent_underlying_content_bytes_ = base::span<const uint8_t>();
  last_sent_partial_content_ = base::span<const std::u16string>();
  last_sent_underlying_content_type_ = lens::MimeType::kUnknown;
  last_sent_page_url_ = GURL();
  num_interaction_requests_sent_ = 0;
}

std::unique_ptr<EndpointFetcher>
TestLensOverlayQueryController::CreateEndpointFetcher(
    lens::LensOverlayServerRequest* request,
    const GURL& fetch_url,
    const std::string& http_method,
    const base::TimeDelta& timeout,
    const std::vector<std::string>& request_headers,
    const std::vector<std::string>& cors_exempt_headers,
    const UploadProgressCallback upload_progress_callback) {
  lens::LensOverlayServerResponse fake_server_response;
  std::string fake_server_response_string;
  google_apis::ApiErrorCode fake_server_response_code =
      google_apis::ApiErrorCode::HTTP_SUCCESS;
  // Whether or not to disable the response.
  bool disable_response = false;
  if (!request) {
    // Cluster info request.
    num_cluster_info_fetch_requests_sent_++;
    fake_server_response_string =
        fake_cluster_info_response_.SerializeAsString();
  } else if (request->has_objects_request() &&
             !request->objects_request().has_image_data() &&
             request->objects_request().has_payload() &&
             request->objects_request().payload().has_partial_pdf_document()) {
    // Partial page content upload request.
    num_partial_page_content_requests_sent_++;
    LOG(ERROR) << num_partial_page_content_requests_sent_;
    sent_partial_page_content_objects_request_.CopyFrom(
        request->objects_request());
    // The server doesn't send a response to this request, so no need to set
    // the response string to something meaningful.
    fake_server_response_string = "";
  } else if (request->has_objects_request() &&
             !request->objects_request().has_image_data() &&
             request->objects_request().has_payload()) {
    // Page content upload request.
    num_page_content_update_requests_sent_++;
    sent_page_content_objects_request_.CopyFrom(request->objects_request());
    // The server doesn't send a response to this request, so no need to set
    // the response string to something meaningful.
    fake_server_response_string = "";
    sent_page_content_request_id_.CopyFrom(
        request->objects_request().request_context().request_id());
  } else if (request->has_objects_request()) {
    // Full image request.
    sent_full_image_objects_request_.CopyFrom(request->objects_request());
    fake_server_response.mutable_objects_response()->CopyFrom(
        fake_objects_response_);
    fake_server_response_string = fake_server_response.SerializeAsString();
    if (next_full_image_request_should_return_error_) {
      fake_server_response_code =
          google_apis::ApiErrorCode::HTTP_INTERNAL_SERVER_ERROR;
    }
    sent_request_id_.CopyFrom(
        request->objects_request().request_context().request_id());
    disable_response = disable_next_objects_response_;
    disable_next_objects_response_ = false;
    next_full_image_request_should_return_error_ = false;
  } else if (request->has_interaction_request()) {
    // Interaction request.
    sent_interaction_request_.CopyFrom(request->interaction_request());
    fake_server_response.mutable_interaction_response()->CopyFrom(
        fake_interaction_response_);
    fake_server_response_string = fake_server_response.SerializeAsString();
    sent_request_id_.CopyFrom(
        request->interaction_request().request_context().request_id());
    num_interaction_requests_sent_++;
  } else {
    NOTREACHED();
  }
  if (request) {
    sent_client_logs_.CopyFrom(request->client_logs());
  }
  sent_fetch_url_ = fetch_url;

  // Create the fake endpoint fetcher to return the fake response.
  EndpointResponse fake_endpoint_response;
  fake_endpoint_response.response = fake_server_response_string;
  fake_endpoint_response.http_status_code = fake_server_response_code;

  if (upload_progress_callback) {
    last_upload_progress_callback_ = std::move(upload_progress_callback);
  }
  // If there is an upload progress callback, run it immedietly with 100%
  // progress, unless disable_page_upload_response_callback in which case the
  // caller will need to call the callback manually.
  if (!disable_page_upload_response_callback &&
      !last_upload_progress_callback_.is_null()) {
    // Simulate the upload progress callback completing the upload.
    std::move(last_upload_progress_callback_).Run(1, 1);
  }

  auto response = std::make_unique<FakeEndpointFetcher>(fake_endpoint_response);
  response->disable_responding_ = disable_response;
  return response;
}

void TestLensOverlayQueryController::SendLatencyGen204IfEnabled(
    lens::LensOverlayGen204Controller::LatencyType latency_type,
    base::TimeTicks start_time_ticks,
    std::string vit_query_param_value,
    std::optional<base::TimeDelta> cluster_info_latency,
    std::optional<std::string> encoded_analytics_id) {
  int counter = latency_gen_204_counter_.contains(latency_type)
                    ? latency_gen_204_counter_.at(latency_type)
                    : 0;
  latency_gen_204_counter_[latency_type] = counter + 1;
}

}  // namespace lens
