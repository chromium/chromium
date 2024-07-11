// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_request_id_generator.h"

#include "base/rand_util.h"
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
  CreateNewAnalyticsId();
}

void LensOverlayRequestIdGenerator::CreateNewAnalyticsId() {
  std::array<uint8_t, kAnalyticsIdBytesSize> bytes;
  base::RandBytes(bytes);
  analytics_id_ = std::string(bytes.begin(), bytes.end());
}

std::unique_ptr<lens::LensOverlayRequestId>
LensOverlayRequestIdGenerator::GetNextRequestId() {
  auto request_id = std::make_unique<lens::LensOverlayRequestId>();
  request_id->set_uuid(uuid_);
  request_id->set_sequence_id(sequence_id_);
  request_id->set_analytics_id(analytics_id_);
  // The server expects the image sequence id to be set, even though
  // it will never change.
  request_id->set_image_sequence_id(1);

  // Increment the sequence id for the next request.
  sequence_id_++;
  return request_id;
}

}  // namespace lens
