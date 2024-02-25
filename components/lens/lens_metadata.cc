// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_metadata.h"

#include <map>

#include "base/base64.h"
#include "base/time/time.h"
#include "components/lens/lens_entrypoints.h"
#include "components/lens/proto/v1/lens_latencies_metadata.pb.h"

namespace {

void AddDownscaleData(lens::proto::lens_latencies_metadata::
                          ChromeSpecificPhaseLatenciesMetadata::Phase* phase,
                      gfx::Size original_size,
                      gfx::Size downscaled_size) {
  lens::proto::lens_latencies_metadata::ChromeSpecificPhaseLatenciesMetadata::
      Phase::ImageDownscaleData* downscale =
          phase->mutable_image_downscale_data();
  downscale->set_original_image_size(original_size.width() *
                                     original_size.height());
  downscale->set_downscaled_image_byte_size(downscaled_size.width() *
                                            downscaled_size.height());
}

void AddEncodeData(lens::proto::lens_latencies_metadata::
                       ChromeSpecificPhaseLatenciesMetadata::Phase* phase,
                   lens::mojom::ImageFormat image_format,
                   size_t encoded_image_size_bytes) {
  lens::proto::lens_latencies_metadata::ChromeSpecificPhaseLatenciesMetadata::
      Phase::ImageEncodeData* encode = phase->mutable_image_encode_data();
  switch (image_format) {
    case (lens::mojom::ImageFormat::ORIGINAL):
      encode->set_original_image_type(
          lens::proto::lens_latencies_metadata::
              ChromeSpecificPhaseLatenciesMetadata::UNKNOWN);
      break;
    case (lens::mojom::ImageFormat::JPEG):
      encode->set_original_image_type(
          lens::proto::lens_latencies_metadata::
              ChromeSpecificPhaseLatenciesMetadata::JPEG);
      break;
    case (lens::mojom::ImageFormat::PNG):
      encode->set_original_image_type(
          lens::proto::lens_latencies_metadata::
              ChromeSpecificPhaseLatenciesMetadata::PNG);
      break;
    case (lens::mojom::ImageFormat::WEBP):
      encode->set_original_image_type(
          lens::proto::lens_latencies_metadata::
              ChromeSpecificPhaseLatenciesMetadata::WEBP);
      break;
  }

  encode->set_encoded_image_size_bytes(encoded_image_size_bytes);
}

}  // namespace

namespace LensMetadata {

std::string CreateProto(
    const std::vector<lens::mojom::LatencyLogPtr>& log_data) {
  lens::proto::lens_latencies_metadata::ChromeSpecificPhaseLatenciesMetadata
      metadata;
  for (const auto& log : log_data) {
    lens::proto::lens_latencies_metadata::ChromeSpecificPhaseLatenciesMetadata::
        Phase* phase = metadata.add_phase();

    lens::proto::lens_latencies_metadata::ChromeSpecificPhaseLatenciesMetadata::
        Phase::Timestamp* timestamp = phase->mutable_timestamp();
    int64_t nanoseconds_in_milliseconds = 1e6;
    int64_t time_nanoseconds =
        log->time.InMillisecondsSinceUnixEpoch() * nanoseconds_in_milliseconds;
    timestamp->set_seconds(time_nanoseconds /
                           base::Time::kNanosecondsPerSecond);
    timestamp->set_nanos(time_nanoseconds % base::Time::kNanosecondsPerSecond);

    switch (log->phase) {
      case lens::mojom::Phase::OVERALL_START:
        phase->set_phase_type(
            lens::proto::lens_latencies_metadata::
                ChromeSpecificPhaseLatenciesMetadata::Phase::OVERALL_START);
        break;
      case lens::mojom::Phase::DOWNSCALE_START:
        phase->set_phase_type(lens::proto::lens_latencies_metadata::
                                  ChromeSpecificPhaseLatenciesMetadata::Phase::
                                      IMAGE_DOWNSCALE_START);
        break;
      case lens::mojom::Phase::DOWNSCALE_END:
        phase->set_phase_type(lens::proto::lens_latencies_metadata::
                                  ChromeSpecificPhaseLatenciesMetadata::Phase::
                                      IMAGE_DOWNSCALE_END);
        AddDownscaleData(phase, log->original_size, log->downscaled_size);
        break;
      case lens::mojom::Phase::ENCODE_START:
        phase->set_phase_type(lens::proto::lens_latencies_metadata::
                                  ChromeSpecificPhaseLatenciesMetadata::Phase::
                                      IMAGE_ENCODE_START);
        break;
      case lens::mojom::Phase::ENCODE_END:
        phase->set_phase_type(
            lens::proto::lens_latencies_metadata::
                ChromeSpecificPhaseLatenciesMetadata::Phase::IMAGE_ENCODE_END);
        AddEncodeData(phase, log->image_format, log->encoded_bytes_size);
        break;
    }
  }

  std::string serialized_proto;
  metadata.SerializeToString(&serialized_proto);

  return base::Base64Encode(serialized_proto);
}

}  // namespace LensMetadata
