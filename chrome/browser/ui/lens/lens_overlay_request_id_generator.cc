// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_request_id_generator.h"

#include "base/containers/span.h"
#include "base/rand_util.h"
#include "components/base32/base32.h"
#include "third_party/lens_server_proto/lens_overlay_request_id.pb.h"

namespace lens {

// The number of bytes to use in an analytics id.
constexpr size_t kAnalyticsIdBytesSize = 16;

LensOverlayRequestIdGenerator::LensOverlayRequestIdGenerator() {
  LensOverlayRequestIdGenerator::ResetRequestId();
}

LensOverlayRequestIdGenerator::~LensOverlayRequestIdGenerator() = default;

void LensOverlayRequestIdGenerator::ResetRequestId() {
  uuid_ = base::RandUint64();
  sequence_id_ = 1;
  image_sequence_id_ = 0;
  CreateNewAnalyticsId();
}

void LensOverlayRequestIdGenerator::CreateNewAnalyticsId() {
  analytics_id_ = base::RandBytesAsString(kAnalyticsIdBytesSize);
}

void LensOverlayRequestIdGenerator::IncrementImageSequenceId() {
  image_sequence_id_++;
}

std::unique_ptr<lens::LensOverlayRequestId>
LensOverlayRequestIdGenerator::GetNextRequestId() {
  auto request_id = std::make_unique<lens::LensOverlayRequestId>();
  request_id->set_uuid(uuid_);
  request_id->set_sequence_id(sequence_id_);
  request_id->set_analytics_id(analytics_id_);
  request_id->set_image_sequence_id(image_sequence_id_);

  // Increment the sequence id for the next request.
  sequence_id_++;
  return request_id;
}

std::string LensOverlayRequestIdGenerator::GetBase32EncodedAnalyticsId() {
  return base32::Base32Encode(base::as_byte_span(analytics_id_),
                              base32::Base32EncodePolicy::OMIT_PADDING);
}

}  // namespace lens
