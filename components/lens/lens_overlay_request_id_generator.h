// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_OVERLAY_REQUEST_ID_GENERATOR_H_
#define COMPONENTS_LENS_LENS_OVERLAY_REQUEST_ID_GENERATOR_H_

#include <memory>
#include <optional>
#include <string>

#include "third_party/lens_server_proto/lens_overlay_request_id.pb.h"
#include "third_party/lens_server_proto/lens_overlay_routing_info.pb.h"

namespace lens {

class TestLensOverlayQueryController;

// The update modes for the request id generator.
enum class RequestIdUpdateMode {
  // Indicates that the request should be modified for the initial set of
  // requests, e.e. incrementing the sequence id, image sequence id, and long
  // context id.
  kInitialRequest = 0,
  // Indicates that the request id should be modified for a full image request,
  // i.e. incrementing the image sequence, sequence id, and creating a new
  // analytics id.
  kFullImageRequest = 1,
  // Indicates that the request id should be modified for a page content
  // request, i.e. incrementing the sequence id and creating a new analytics
  // id.
  kPageContentRequest = 2,
  // Indicates that the request id should be modified for a partial page content
  // request, i.e. incrementing the sequence id and creating a new analytics
  // id.
  kPartialPageContentRequest = 3,
  // Indicates that the request id should be modified for an interaction
  // request, i.e. incrementing the sequence id and creating a new analytics
  // id.
  kInteractionRequest = 4,
  // Indicates that the request id should be modified for a search url.
  // i.e. just incrementing the sequence id.
  kSearchUrl = 5,
  // Indicates that the request id should be modified for opening in a new tab.
  // i.e. just creating a new analytics id, but not storing it for future
  // updates.
  kOpenInNewTab = 6,
  // Indicates that the request id should be modified for a page content
  // request with a viewport screenshot, i.e. incrementing the sequence id,
  // image sequence id, long context id, and creating a new analytics id.
  kPageContentWithViewportRequest = 7,
  // Indicates that the request id should be modified for a new context upload
  // in a multi-context upload flow, i.e. resetting the sequence id, image
  // sequence id, and creating a new uuid and analytics id, regardless of the
  // context upload mime type.
  kMultiContextUploadRequest = 8,
};

// Manages creating lens overlay request IDs. Owned by a single Lens overlay
// query controller.
class LensOverlayRequestIdGenerator {
 public:
  LensOverlayRequestIdGenerator();
  ~LensOverlayRequestIdGenerator();

  // Decodes a base64 encoded request id and returns the proto as a unique_ptr.
  // Returns nullptr if the decoding or parsing fails.
  static std::unique_ptr<lens::LensOverlayRequestId> ParseRequestId(
      const std::string& encoded_request_id);

  // Resets the request id generator, creating a new uuid and resetting the
  // sequence.
  void ResetRequestId();

  // Updates the request id based on the given update mode and returns the
  // request id proto.
  // TODO(crbug.com/489187358): Remove this method once all callers are migrated
  // to the overload that uses mime type, and remove the _AND_IMAGE media types
  // from the proto definition.
  std::unique_ptr<lens::LensOverlayRequestId> GetNextRequestId(
      RequestIdUpdateMode update_mode,
      lens::LensOverlayRequestId::MediaType media_type);

  // Updates the request id based on the given update mode and returns the
  // request id proto. Uses the mime type to determine the media type.
  std::unique_ptr<lens::LensOverlayRequestId> GetNextRequestId(
      RequestIdUpdateMode update_mode,
      std::string mime_type);

  // Creates a new request id based on the previous request id and update mode.
  // This does not modify the generator's internal state.
  // TODO(crbug.com/472498582): Migrate all callers of GetNextRequestId to
  // call this method and remove most internal state from this class.
  std::unique_ptr<lens::LensOverlayRequestId> CreateNextRequestIdForUpdate(
      std::unique_ptr<lens::LensOverlayRequestId> previous_request_id,
      RequestIdUpdateMode update_mode);

  // Returns the current analytics id as a base32 encoded string.
  std::string GetBase32EncodedAnalyticsId();

  // Updates the context id returned in future request ids with the given
  // context id.
  void SetContextId(int64_t context_id) { context_id_ = context_id; }

  // Updates the has_chrome_tab_data field in future request ids.
  void SetHasChromeTabData(bool has_chrome_tab_data) {
    has_chrome_tab_data_ = has_chrome_tab_data;
  }

  // Updates the is_implicit_upload field in future request ids.
  void SetIsImplicitUpload(bool is_implicit_upload) {
    is_implicit_upload_ = is_implicit_upload;
  }

  // Sets the routing info to be included in the request id and returns the new
  // request id with this routing info.
  std::unique_ptr<lens::LensOverlayRequestId> SetRoutingInfo(
      lens::LensOverlayRoutingInfo routing_info);

  bool HasRoutingInfo() { return routing_info_.has_value(); }

 protected:
  friend class TestLensOverlayQueryController;
  // Returns the request id of the current requests stored in the request id
  // generator.
  std::unique_ptr<lens::LensOverlayRequestId> GetCurrentRequestIdForTesting() {
    return GetCurrentRequestId();
  }

 private:
  // Updates the internal state of the request id generator based on the given
  // request id.
  void UpdateInternalStateFromRequestId(
      const lens::LensOverlayRequestId& request_id,
      bool save_analytics_id);

  // Returns the request id of the current requests stored in the request id
  // generator.
  std::unique_ptr<lens::LensOverlayRequestId> GetCurrentRequestId();

  // The current uuid. Valid for the duration of a Lens overlay session.
  uint64_t uuid_;

  // The analytics id for the current request. Will be updated on each
  // query.
  std::string analytics_id_;

  // The current sequence id.
  int sequence_id_;

  // The current image sequence id.
  int image_sequence_id_;

  // The current long context id.
  int long_context_id_;

  // The context ID to use for the request ID. This is generated once and
  // reused for all requests.
  int64_t context_id_;

  // Whether the request id has Chrome tab data.
  bool has_chrome_tab_data_;

  // Whether the request id is for an implicit upload.
  // e.g. a viewport screenshot from the Lens overlay contextual searchbox.
  bool is_implicit_upload_;

  // The mime type string.
  std::optional<std::string> mime_type_;

  // The current routing info. Not guaranteed to exist if not returned from the
  // server.
  std::optional<lens::LensOverlayRoutingInfo> routing_info_;
};

}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_OVERLAY_REQUEST_ID_GENERATOR_H_
