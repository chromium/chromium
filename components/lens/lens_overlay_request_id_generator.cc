// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_overlay_request_id_generator.h"

#include "base/check.h"
#include "base/containers/span.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "components/base32/base32.h"
#include "components/lens/lens_features.h"
#include "lens_overlay_request_id_generator.h"
#include "third_party/lens_server_proto/lens_overlay_request_id.pb.h"

namespace lens {

// The number of bytes to use in an analytics id.
constexpr size_t kAnalyticsIdBytesSize = 16;

namespace {

int64_t RandInt64() {
  int64_t number;
  base::RandBytes(base::byte_span_from_ref(number));
  return number >> 1;
}

}  // namespace

LensOverlayRequestIdGenerator::LensOverlayRequestIdGenerator() {
  LensOverlayRequestIdGenerator::ResetRequestId();
}

LensOverlayRequestIdGenerator::~LensOverlayRequestIdGenerator() = default;

void LensOverlayRequestIdGenerator::ResetRequestId() {
  uuid_ = base::RandUint64();
  sequence_id_ = 0;
  image_sequence_id_ = 0;
  long_context_id_ = 0;
  analytics_id_ = base::RandBytesAsString(kAnalyticsIdBytesSize);
  context_id_ = RandInt64();
  routing_info_.reset();
}

std::unique_ptr<lens::LensOverlayRequestId>
LensOverlayRequestIdGenerator::GetNextRequestId(
    RequestIdUpdateMode update_mode,
    lens::LensOverlayRequestId::MediaType media_type,
    std::optional<int64_t> context_id) {
  // Verify that the initial request id is only generated once.
  CHECK(update_mode != RequestIdUpdateMode::kInitialRequest ||
        sequence_id_ == 0);

  bool store_analytics_id = update_mode != RequestIdUpdateMode::kOpenInNewTab;

  std::unique_ptr<lens::LensOverlayRequestId> current_request_id =
      GetCurrentRequestId();
  current_request_id->set_media_type(media_type);
  std::unique_ptr<lens::LensOverlayRequestId> next_request_id =
      CreateNextRequestIdForUpdate(std::move(current_request_id), update_mode);

  // Update the internal state.
  uuid_ = next_request_id->uuid();
  sequence_id_ = next_request_id->sequence_id();
  image_sequence_id_ = next_request_id->image_sequence_id();
  long_context_id_ = next_request_id->long_context_id();
  if (store_analytics_id) {
    analytics_id_ = next_request_id->analytics_id();
  }

  if (context_id.has_value()) {
    next_request_id->set_context_id(context_id.value());
  }
  return next_request_id;
}

std::unique_ptr<lens::LensOverlayRequestId>
LensOverlayRequestIdGenerator::CreateNextRequestIdForUpdate(
    std::unique_ptr<lens::LensOverlayRequestId> previous_request_id,
    RequestIdUpdateMode update_mode) {
  auto request_id = std::make_unique<lens::LensOverlayRequestId>();
  request_id->set_uuid(previous_request_id->uuid());
  request_id->set_sequence_id(previous_request_id->sequence_id());
  request_id->set_analytics_id(previous_request_id->analytics_id());
  request_id->set_long_context_id(previous_request_id->long_context_id());
  request_id->set_image_sequence_id(previous_request_id->image_sequence_id());
  request_id->set_media_type(previous_request_id->media_type());
  request_id->set_time_usec(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  if (previous_request_id->has_routing_info()) {
    request_id->mutable_routing_info()->CopyFrom(
        previous_request_id->routing_info());
  }
  request_id->set_context_id(previous_request_id->context_id());

  bool create_analytics_id =
      update_mode != RequestIdUpdateMode::kSearchUrl &&
      update_mode != RequestIdUpdateMode::kPartialPageContentRequest;

  if (update_mode == RequestIdUpdateMode::kMultiContextUploadRequest) {
    // kMultiContextUploadRequest is only used for the initial request in a
    // multi-context upload flow, so reset all ids to their initial values.
    request_id->set_uuid(base::RandUint64());
    request_id->set_image_sequence_id(1);
    request_id->set_sequence_id(1);
    // All media types other than image-only should set long-context-id to 1.
    request_id->set_long_context_id(
        previous_request_id->media_type() ==
                LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE
            ? 0
            : 1);
  } else {
    bool increment_image_sequence =
        update_mode == RequestIdUpdateMode::kFullImageRequest ||
        update_mode == RequestIdUpdateMode::kPageContentWithViewportRequest ||
        update_mode == RequestIdUpdateMode::kInitialRequest;
    bool increment_sequence = update_mode != RequestIdUpdateMode::kOpenInNewTab;
    bool increment_long_context =
        update_mode == RequestIdUpdateMode::kPageContentRequest ||
        update_mode == RequestIdUpdateMode::kPageContentWithViewportRequest ||
        update_mode == RequestIdUpdateMode::kInitialRequest;

    if (increment_image_sequence) {
      request_id->set_image_sequence_id(request_id->image_sequence_id() + 1);
    }
    if (increment_sequence) {
      request_id->set_sequence_id(request_id->sequence_id() + 1);
    }
    if (increment_long_context) {
      request_id->set_long_context_id(request_id->long_context_id() + 1);
    }
  }

  if (create_analytics_id) {
    request_id->set_analytics_id(
        base::RandBytesAsString(kAnalyticsIdBytesSize));
  }

  return request_id;
}

std::unique_ptr<lens::LensOverlayRequestId>
LensOverlayRequestIdGenerator::GetRequestIdWithMultiContextId(
    lens::LensOverlayRequestId::MediaType media_type,
    int64_t context_id) {
  // The request ID flow for the multi-context upload flow using context_id
  // is intended have separate request ids for viewport vs content upload
  // requests, so the media type should never combine the two with _AND_IMAGE.
  // Instead, the caller should call this method separately for the content
  // and viewport upload requests, and provide the same context_id for both.
  CHECK(media_type != LensOverlayRequestId::MEDIA_TYPE_PDF_AND_IMAGE &&
        media_type != LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE);

  auto request_id = std::make_unique<lens::LensOverlayRequestId>();
  request_id->set_uuid(base::RandUint64());
  request_id->set_time_usec(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  request_id->set_media_type(media_type);
  if (routing_info_.has_value()) {
    request_id->mutable_routing_info()->CopyFrom(routing_info_.value());
  }
  request_id->set_context_id(context_id);
  return request_id;
}

std::string LensOverlayRequestIdGenerator::GetBase32EncodedAnalyticsId() {
  return base32::Base32Encode(base::as_byte_span(analytics_id_),
                              base32::Base32EncodePolicy::OMIT_PADDING);
}

std::unique_ptr<lens::LensOverlayRequestId>
LensOverlayRequestIdGenerator::SetRoutingInfo(
    lens::LensOverlayRoutingInfo routing_info) {
  routing_info_ = routing_info;
  return GetCurrentRequestId();
}

std::unique_ptr<lens::LensOverlayRequestId>
LensOverlayRequestIdGenerator::GetCurrentRequestId() {
  auto request_id = std::make_unique<lens::LensOverlayRequestId>();
  request_id->set_uuid(uuid_);
  request_id->set_sequence_id(sequence_id_);
  request_id->set_analytics_id(analytics_id_);
  request_id->set_long_context_id(long_context_id_);
  request_id->set_image_sequence_id(image_sequence_id_);
  request_id->set_time_usec(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  if (routing_info_.has_value()) {
    request_id->mutable_routing_info()->CopyFrom(routing_info_.value());
  }
  request_id->set_context_id(context_id_);
  return request_id;
}
}  // namespace lens
