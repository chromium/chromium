// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_TEST_LENS_OVERLAY_QUERY_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_TEST_LENS_OVERLAY_QUERY_CONTROLLER_H_

#include "lens_overlay_query_controller.h"

namespace lens {

class FakeEndpointFetcher : public EndpointFetcher {
 public:
  explicit FakeEndpointFetcher(EndpointResponse response);
  void PerformRequest(EndpointFetcherCallback endpoint_fetcher_callback,
                      const char* key) override;

  bool disable_responding_ = false;

 private:
  EndpointResponse response_;
};

// Helper for testing features that use the LensOverlayQueryController.
// The only logic in this class should be for setting up fake network responses
// and tracking sent request data to maximize testing coverage.
class TestLensOverlayQueryController : public LensOverlayQueryController {
 public:
  explicit TestLensOverlayQueryController(
      LensOverlayFullImageResponseCallback full_image_callback,
      LensOverlayUrlResponseCallback url_callback,
      LensOverlaySuggestInputsCallback interaction_data_callback,
      LensOverlayThumbnailCreatedCallback thumbnail_created_callback,
      variations::VariationsClient* variations_client,
      signin::IdentityManager* identity_manager,
      Profile* profile,
      lens::LensOverlayInvocationSource invocation_source,
      bool use_dark_mode,
      lens::LensOverlayGen204Controller* gen204_controller);
  ~TestLensOverlayQueryController() override;

  // Mutators.
  void set_fake_cluster_info_response(
      lens::LensOverlayServerClusterInfoResponse response) {
    fake_cluster_info_response_ = response;
  }

  void set_fake_objects_response(lens::LensOverlayObjectsResponse response) {
    fake_objects_response_ = response;
  }

  void set_fake_interaction_response(
      lens::LensOverlayInteractionResponse response) {
    fake_interaction_response_ = response;
  }

  void set_disable_next_objects_response(bool disable_next_objects_response) {
    disable_next_objects_response_ = disable_next_objects_response;
  }

  void set_next_full_image_request_should_return_error(
      bool next_full_image_request_should_return_error) {
    next_full_image_request_should_return_error_ =
        next_full_image_request_should_return_error;
  }

  void set_disable_page_upload_response_callback(bool disable) {
    disable_page_upload_response_callback = disable;
  }

  void RunUploadProgressCallback() {
    std::move(last_upload_progress_callback_).Run(1, 1);
  }

  // Accessors.
  const GURL& sent_fetch_url() const { return sent_fetch_url_; }

  const lens::LensOverlayClientLogs& sent_client_logs() const {
    return sent_client_logs_;
  }

  const lens::LensOverlayRequestId& sent_request_id() const {
    return sent_request_id_;
  }

  const lens::LensOverlayRequestId& sent_page_content_request_id() const {
    return sent_page_content_request_id_;
  }

  const lens::LensOverlayObjectsRequest& sent_full_image_objects_request()
      const {
    return sent_full_image_objects_request_;
  }

  const lens::LensOverlayObjectsRequest& sent_page_content_objects_request()
      const {
    return sent_page_content_objects_request_;
  }

  const lens::LensOverlayObjectsRequest&
  sent_partial_page_content_objects_request() const {
    return sent_partial_page_content_objects_request_;
  }

  const lens::LensOverlayInteractionRequest& sent_interaction_request() const {
    return sent_interaction_request_;
  }

  const std::string& last_queried_text() const { return last_queried_text_; }

  const lens::LensOverlaySelectionType& last_lens_selection_type() const {
    return last_lens_selection_type_;
  }

  const lens::mojom::CenterRotatedBoxPtr& last_queried_region() const {
    return last_queried_region_;
  }

  const std::optional<SkBitmap>& last_queried_region_bytes() const {
    return last_queried_region_bytes_;
  }

  base::span<const uint8_t> last_sent_underlying_content_bytes() const {
    return last_sent_underlying_content_bytes_;
  }

  const lens::MimeType& last_sent_underlying_content_type() const {
    return last_sent_underlying_content_type_;
  }

  base::span<const std::u16string> last_sent_partial_content() const {
    return last_sent_partial_content_;
  }

  const GURL& last_sent_page_url() const { return last_sent_page_url_; }

  const std::optional<lens::mojom::UserAction>& last_user_action() const {
    return last_user_action_;
  }

  const int& num_interaction_requests_sent() const {
    return num_interaction_requests_sent_;
  }

  const int& num_cluster_info_fetch_requests_sent() const {
    return num_cluster_info_fetch_requests_sent_;
  }

  const int& num_full_page_objects_gen204_pings_sent() const {
    return num_full_page_objects_gen204_pings_sent_;
  }

  const int& num_partial_page_content_requests_sent() const {
    return num_partial_page_content_requests_sent_;
  }

  int latency_gen_204_counter(
      lens::LensOverlayGen204Controller::LatencyType latency_type) const {
    auto it = latency_gen_204_counter_.find(latency_type);
    return it == latency_gen_204_counter_.end() ? 0 : it->second;
  }

  void StartQueryFlow(
      const SkBitmap& screenshot,
      GURL page_url,
      std::optional<std::string> page_title,
      std::vector<lens::mojom::CenterRotatedBoxPtr> significant_region_boxes,
      base::span<const uint8_t> underlying_content_bytes,
      lens::MimeType underlying_content_type,
      float ui_scale_factor,
      base::TimeTicks invocation_time) override;

  void SendTaskCompletionGen204IfEnabled(
      lens::mojom::UserAction user_action) override;

  void SendRegionSearch(
      lens::mojom::CenterRotatedBoxPtr region,
      lens::LensOverlaySelectionType selection_type,
      std::map<std::string, std::string> additional_search_query_params,
      std::optional<SkBitmap> region_bytes) override;

  void SendTextOnlyQuery(const std::string& query_text,
                         lens::LensOverlaySelectionType lens_selection_type,
                         std::map<std::string, std::string>
                             additional_search_query_params) override;

  void SendMultimodalRequest(
      lens::mojom::CenterRotatedBoxPtr region,
      const std::string& query_text,
      lens::LensOverlaySelectionType multimodal_selection_type,
      std::map<std::string, std::string> additional_search_query_params,
      std::optional<SkBitmap> region_bitmap) override;

  void SendContextualTextQuery(
      const std::string& query_text,
      lens::LensOverlaySelectionType lens_selection_type,
      std::map<std::string, std::string> additional_search_query_params)
      override;

  void SendPageContentUpdateRequest(base::span<const uint8_t> new_content_bytes,
                                    lens::MimeType new_content_type,
                                    GURL new_page_url) override;

  void SendPartialPageContentRequest(
      base::span<const std::u16string> partial_content) override;

  // Resets the test state.
  void ResetTestingState();

 protected:
  std::unique_ptr<EndpointFetcher> CreateEndpointFetcher(
      lens::LensOverlayServerRequest* request,
      const GURL& fetch_url,
      const std::string& http_method,
      const base::TimeDelta& timeout,
      const std::vector<std::string>& request_headers,
      const std::vector<std::string>& cors_exempt_headers,
      const UploadProgressCallback upload_progress_callback) override;

  void SendLatencyGen204IfEnabled(
      lens::LensOverlayGen204Controller::LatencyType latency_type,
      base::TimeTicks start_time_ticks,
      std::string vit_query_param_value,
      std::optional<base::TimeDelta> cluster_info_latency,
      std::optional<std::string> encoded_analytics_id) override;

  // The fake response to return for cluster info requests.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response_;

  // The fake response to return for objects requests.
  lens::LensOverlayObjectsResponse fake_objects_response_;

  // The fake response to return for interaction requests.
  lens::LensOverlayInteractionResponse fake_interaction_response_;

  // If true, the response for the next objects request will not be returned.
  bool disable_next_objects_response_ = false;

  // If true, the next full image request will return an error.
  bool next_full_image_request_should_return_error_ = false;

  // If true, the CreateEndpointFetcher will not automatically respond with a
  // complete upload to the UploadProgressCallback.
  bool disable_page_upload_response_callback = false;

  // The last url for which a fetch request was sent by the query controller.
  GURL sent_fetch_url_;

  // The last client logs sent by the query controller.
  lens::LensOverlayClientLogs sent_client_logs_;

  // The last request id sent by the query controller.
  lens::LensOverlayRequestId sent_request_id_;

  // The last request id sent by the query controller for a page content upload
  // request.
  lens::LensOverlayRequestId sent_page_content_request_id_;

  // The last full image objects request sent by the query controller.
  lens::LensOverlayObjectsRequest sent_full_image_objects_request_;

  // The last page content objects request sent by the query controller.
  lens::LensOverlayObjectsRequest sent_page_content_objects_request_;

  // The last partial page content objects request sent by the query controller.
  lens::LensOverlayObjectsRequest sent_partial_page_content_objects_request_;

  // The last interaction request sent by the query controller.
  lens::LensOverlayInteractionRequest sent_interaction_request_;

  // The last query text sent by the query controller.
  std::string last_queried_text_;

  // The last lens selection type sent by the query controller.
  lens::LensOverlaySelectionType last_lens_selection_type_;

  // The last region sent by the query controller.
  lens::mojom::CenterRotatedBoxPtr last_queried_region_;

  // The last region bytes sent by the query controller.
  std::optional<SkBitmap> last_queried_region_bytes_;

  // The last underlying content bytes sent by the query controller.
  base::raw_span<const uint8_t> last_sent_underlying_content_bytes_;

  // The last underlying content type sent by the query controller.
  lens::MimeType last_sent_underlying_content_type_;

  // The last partial content sent by the query controller.
  base::raw_span<const std::u16string> last_sent_partial_content_;

  // The last page url sent by the query controller.
  GURL last_sent_page_url_;

  // The last user action sent by the query controller.
  std::optional<lens::mojom::UserAction> last_user_action_;

  // The number of interaction requests sent by the query controller.
  int num_interaction_requests_sent_ = 0;

  // The number of cluster info fetch requests sent by the query controller.
  int num_cluster_info_fetch_requests_sent_ = 0;

  // The number of full page objects gen204 pings sent by the query controller.
  int num_full_page_objects_gen204_pings_sent_ = 0;

  // The number of full page translate gen204 pings sent by the query
  // controller.
  int num_full_page_translate_gen204_pings_sent_ = 0;

  // The number of page content update requests sent by the query controller.
  int num_page_content_update_requests_sent_ = 0;

  // The number of partial page content requests sent by the query controller.
  int num_partial_page_content_requests_sent_ = 0;

  // Tracker for the number of latency request events sent by the query
  // controller.
  base::flat_map<lens::LensOverlayGen204Controller::LatencyType, int>
      latency_gen_204_counter_;

  // The last upload progress callback sent by the query controller.
  UploadProgressCallback last_upload_progress_callback_;
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_TEST_LENS_OVERLAY_QUERY_CONTROLLER_H_
