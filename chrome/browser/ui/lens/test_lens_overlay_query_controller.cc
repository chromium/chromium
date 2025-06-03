// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test_lens_overlay_query_controller.h"

#include "base/base64url.h"
#include "base/containers/span.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/protobuf_matchers.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "google_apis/common/api_error_codes.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/lens_server_proto/lens_overlay_service_deps.pb.h"

using base::test::EqualsProto;
using endpoint_fetcher::EndpointFetcher;
using endpoint_fetcher::EndpointFetcherCallback;
using endpoint_fetcher::EndpointResponse;
using endpoint_fetcher::HttpMethod;

namespace lens {

constexpr char kPdfMimeType[] = "application/pdf";
constexpr char kPlainTextMimeType[] = "text/plain";
constexpr char kHtmlMimeType[] = "text/html";

lens::MimeType StringToContentType(const std::string& content_type) {
  if (content_type == kPdfMimeType) {
    return lens::MimeType::kPdf;
  } else if (content_type == kHtmlMimeType) {
    return lens::MimeType::kHtml;
  } else if (content_type == kPlainTextMimeType) {
    return lens::MimeType::kPlainText;
  }
  return lens::MimeType::kUnknown;
}

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
    LensOverlayInteractionResponseCallback interaction_callback,
    LensOverlaySuggestInputsCallback interaction_data_callback,
    LensOverlayThumbnailCreatedCallback thumbnail_created_callback,
    UploadProgressCallback upload_progress_callback,
    variations::VariationsClient* variations_client,
    signin::IdentityManager* identity_manager,
    Profile* profile,
    lens::LensOverlayInvocationSource invocation_source,
    bool use_dark_mode,
    lens::LensOverlayGen204Controller* gen204_controller)
    : LensOverlayQueryController(full_image_callback,
                                 url_callback,
                                 interaction_callback,
                                 interaction_data_callback,
                                 thumbnail_created_callback,
                                 upload_progress_callback,
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
    base::span<const lens::PageContent> underlying_page_contents,
    lens::MimeType primary_content_type,
    std::optional<uint32_t> pdf_current_page,
    float ui_scale_factor,
    base::TimeTicks invocation_time) {
  // Deep copy significant_region_boxes to avoid lifetime issues after the
  // std::move call below.
  last_sent_significant_region_boxes_.clear();
  for (const auto& box : significant_region_boxes) {
    last_sent_significant_region_boxes_.push_back(box.Clone());
  }

  LensOverlayQueryController::StartQueryFlow(
      screenshot, page_url, page_title, std::move(significant_region_boxes),
      underlying_page_contents, primary_content_type, pdf_current_page,
      ui_scale_factor, invocation_time);
}

void TestLensOverlayQueryController::SendRegionSearch(
    base::Time query_start_time,
    lens::mojom::CenterRotatedBoxPtr region,
    lens::LensOverlaySelectionType selection_type,
    std::map<std::string, std::string> additional_search_query_params,
    std::optional<SkBitmap> region_bytes) {
  last_queried_region_ = region->Clone();
  last_queried_region_bytes_ = region_bytes;
  last_lens_selection_type_ = selection_type;

  LensOverlayQueryController::SendRegionSearch(
      query_start_time, std::move(region), selection_type,
      additional_search_query_params, region_bytes);
}

void TestLensOverlayQueryController::SendTextOnlyQuery(
    base::Time query_start_time,
    const std::string& query_text,
    lens::LensOverlaySelectionType lens_selection_type,
    std::map<std::string, std::string> additional_search_query_params) {
  last_queried_text_ = query_text;
  last_lens_selection_type_ = lens_selection_type;

  LensOverlayQueryController::SendTextOnlyQuery(query_start_time, query_text,
                                                lens_selection_type,
                                                additional_search_query_params);
}

void TestLensOverlayQueryController::SendMultimodalRequest(
    base::Time query_start_time,
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
      query_start_time, std::move(region), query_text,
      multimodal_selection_type, additional_search_query_params, region_bitmap);
}

void TestLensOverlayQueryController::SendContextualTextQuery(
    base::Time query_start_time,
    const std::string& query_text,
    lens::LensOverlaySelectionType lens_selection_type,
    std::map<std::string, std::string> additional_search_query_params) {
  last_queried_text_ = query_text;
  last_lens_selection_type_ = lens_selection_type;

  LensOverlayQueryController::SendContextualTextQuery(
      query_start_time, query_text, lens_selection_type,
      additional_search_query_params);
}

void TestLensOverlayQueryController::ResetTestingState() {
  last_lens_selection_type_ = lens::UNKNOWN_SELECTION_TYPE;
  last_queried_region_.reset();
  last_queried_text_.clear();
  last_queried_region_bytes_ = std::nullopt;
  last_sent_underlying_content_bytes_ = base::span<const uint8_t>();
  last_sent_page_content_payload_ = lens::Payload();
  last_sent_partial_content_ = lens::LensOverlayDocument();
  last_sent_underlying_content_type_ = lens::MimeType::kUnknown;
  last_sent_page_content_data_.clear();
  last_sent_page_url_ = GURL();
  num_interaction_requests_sent_ = 0;
  num_upload_chunk_requests_sent_ = 0;
  last_cluster_info_request_ = std::nullopt;
}

std::unique_ptr<EndpointFetcher>
TestLensOverlayQueryController::CreateEndpointFetcher(
    std::string request_string,
    const GURL& fetch_url,
    HttpMethod http_method,
    base::TimeDelta timeout,
    const std::vector<std::string>& request_headers,
    const std::vector<std::string>& cors_exempt_headers,
    UploadProgressCallback upload_progress_callback) {
  lens::LensOverlayServerResponse fake_server_response;
  std::string fake_server_response_string;
  google_apis::ApiErrorCode fake_server_response_code =
      google_apis::ApiErrorCode::HTTP_SUCCESS;
  // Whether or not to disable the response.
  bool disable_response = false;

  lens::LensOverlayServerRequest request;
  request.ParseFromString(request_string);

  const auto chunk_endpoint_url =
      GURL(lens::features::GetLensOverlayUploadChunkEndpointURL());
  bool is_chunk_request =
      chunk_endpoint_url.GetWithEmptyPath() == fetch_url.GetWithEmptyPath() &&
      chunk_endpoint_url.path() == fetch_url.path();
  bool is_cluster_info_request =
      fetch_url == GURL(lens::features::GetLensOverlayClusterInfoEndpointUrl());

  if (is_cluster_info_request) {
    // Cluster info request.
    num_cluster_info_fetch_requests_sent_++;
    fake_server_response_string =
        fake_cluster_info_response_.SerializeAsString();
    if (!request_string.empty()) {
      lens::LensOverlayServerClusterInfoRequest cluster_info_request;
      cluster_info_request.ParseFromString(request_string);
      last_cluster_info_request_ = cluster_info_request;
    }
  } else if (is_chunk_request) {
    // Upload chunk request.
    lens::LensOverlayUploadChunkResponse fake_response;
    num_upload_chunk_requests_sent_++;
    fake_server_response_string = fake_response.SerializeAsString();
  } else if (request.has_objects_request() &&
             !request.objects_request().has_image_data() &&
             request.objects_request().has_payload() &&
             request.objects_request().payload().request_type() ==
                 lens::RequestType::REQUEST_TYPE_EARLY_PARTIAL_PDF) {
    // Partial page content upload request.
    num_partial_page_content_requests_sent_++;
    sent_partial_page_content_objects_request_.CopyFrom(
        request.objects_request());
    // The server doesn't send a response to this request, so no need to set
    // the response string to something meaningful.
    fake_server_response_string = "";
    if (request.objects_request().payload().has_partial_pdf_document()) {
      last_sent_partial_content_.CopyFrom(
          request.objects_request().payload().partial_pdf_document());
    } else {
      lens::LensOverlayDocument partial_pdf_document;
      partial_pdf_document.ParseFromString(
          request.objects_request().payload().content().content_data(0).data());
      last_sent_partial_content_.CopyFrom(partial_pdf_document);
    }
  } else if (request.has_objects_request() &&
             !request.objects_request().has_image_data() &&
             request.objects_request().has_payload()) {
    // Page content upload request.
    num_page_content_update_requests_sent_++;
    sent_page_content_objects_request_.CopyFrom(request.objects_request());
    last_sent_page_content_payload_.CopyFrom(
        request.objects_request().payload());
    // The server doesn't send a response to this request, so no need to set
    // the response string to something meaningful.
    fake_server_response_string = "";
    sent_page_content_request_id_.CopyFrom(
        request.objects_request().request_context().request_id());
    // Need to reset the underlying content bytes before changing
    // last_sent_page_content_data_ to prevent a dangling reference.
    last_sent_underlying_content_bytes_ = {};
    last_sent_page_content_data_ =
        std::string(request.objects_request().payload().content_data());
    last_sent_underlying_content_bytes_ =
        base::as_byte_span(last_sent_page_content_data_);
    last_sent_underlying_content_type_ =
        StringToContentType(request.objects_request().payload().content_type());
    last_sent_page_url_ =
        GURL(request.objects_request().payload().content().webpage_url());
  } else if (request.has_objects_request()) {
    // Full image request.
    num_full_image_requests_sent_++;
    sent_full_image_objects_request_.CopyFrom(request.objects_request());
    fake_server_response.mutable_objects_response()->CopyFrom(
        fake_objects_response_);
    fake_server_response_string = fake_server_response.SerializeAsString();
    if (next_full_image_request_should_return_error_) {
      fake_server_response_code =
          google_apis::ApiErrorCode::HTTP_INTERNAL_SERVER_ERROR;
    }
    sent_full_image_request_id_.CopyFrom(
        request.objects_request().request_context().request_id());
    disable_response = disable_next_objects_response_;
    disable_next_objects_response_ = false;
    next_full_image_request_should_return_error_ = false;
  } else if (request.has_interaction_request()) {
    // Interaction request.
    sent_interaction_request_.CopyFrom(request.interaction_request());
    fake_server_response.mutable_interaction_response()->CopyFrom(
        fake_interaction_response_);
    fake_server_response_string = fake_server_response.SerializeAsString();
    sent_interaction_request_id_.CopyFrom(
        request.interaction_request().request_context().request_id());
    num_interaction_requests_sent_++;
  } else {
    NOTREACHED();
  }
  if (!request_string.empty()) {
    sent_client_logs_.CopyFrom(request.client_logs());
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
    std::optional<std::string> encoded_analytics_id,
    std::optional<lens::LensOverlayRequestId> request_id) {
  int counter = latency_gen_204_counter_.contains(latency_type)
                    ? latency_gen_204_counter_.at(latency_type)
                    : 0;
  latency_gen_204_counter_[latency_type] = counter + 1;
  last_latency_gen204_analytics_id_ = encoded_analytics_id;
  last_latency_gen204_request_id_ = request_id;
}

void TestLensOverlayQueryController::SendTaskCompletionGen204IfEnabled(
    std::string encoded_analytics_id,
    lens::mojom::UserAction user_action,
    lens::LensOverlayRequestId request_id) {
  last_user_action_ = user_action;
  last_task_completion_gen204_analytics_id_ = encoded_analytics_id;
  last_task_completion_gen204_request_id_ =
      std::make_optional<lens::LensOverlayRequestId>(request_id);
}

void TestLensOverlayQueryController::SendSemanticEventGen204IfEnabled(
    lens::mojom::SemanticEvent event,
    std::optional<lens::LensOverlayRequestId> request_id) {
  last_semantic_event_ = event;
  last_semantic_event_gen204_request_id_ = request_id;
}

void TestLensOverlayQueryController::RunSuggestInputsCallback() {
  const lens::proto::LensOverlaySuggestInputs& last_suggest_inputs =
      suggest_inputs_for_testing();
  if (last_suggest_inputs.encoded_request_id().empty()) {
    LensOverlayQueryController::RunSuggestInputsCallback();
    return;
  }

  // Decode the request id from the SuggestInputs callback.
  lens::LensOverlayRequestId latest_request_id;
  std::string serialized_proto;
  EXPECT_TRUE(base::Base64UrlDecode(
      last_suggest_inputs.encoded_request_id(),
      base::Base64UrlDecodePolicy::DISALLOW_PADDING, &serialized_proto));
  EXPECT_TRUE(latest_request_id.ParseFromString(serialized_proto));

  // Get the current request id from the request id generator.
  std::unique_ptr<lens::LensOverlayRequestId> current_request_id =
      request_id_generator_for_testing()->GetCurrentRequestIdForTesting();
  // Verifies that the last request ids passed in the SuggestInputs callback are
  // the same as current request id in the request id generator.
  // This is to ensure the LensOverlayController is always updated with the
  // latest request ids.
  EXPECT_THAT(latest_request_id, EqualsProto(*current_request_id))
      << "The latest request id passed in the SuggestInputs callback is not "
         "the same as the current request id in the request id generator. Did "
         "you call RunSuggestInputsCallback() after updating the request id?";

  LensOverlayQueryController::RunSuggestInputsCallback();
}
}  // namespace lens
