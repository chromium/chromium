// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search/ntp_composebox_fieldtrial.h"

#include <string>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"

namespace ntp_composebox_fieldtrial {

namespace {

// Decodes a proto object from its serialized Base64 string representation.
// Returns true if decoding and parsing succeed, false otherwise.
bool ParseProtoFromBase64String(const std::string& input,
                                google::protobuf::MessageLite& output) {
  if (input.empty()) {
    return false;
  }

  std::string decoded_input;
  // Decode the Base64-encoded input string into decoded_input.
  if (!base::Base64Decode(input, &decoded_input)) {
    return false;
  }

  if (decoded_input.empty()) {
    return false;
  }

  // Parse the decoded string into the proto object.
  return output.ParseFromString(decoded_input);
}

// Populates and returns the NTP Composebox configuration proto.
omnibox::NTPComposeboxConfig GetNTPComposeboxConfig() {
  // Initialize the default config.
  omnibox::NTPComposeboxConfig default_config;
  default_config.mutable_entry_point()->set_num_page_load_animations(3);

  // Attempt to parse the config proto from the feature parameter if it is set.
  omnibox::NTPComposeboxConfig fieldtrial_config;
  if (!kConfigParam.Get().empty()) {
    bool parsed =
        ParseProtoFromBase64String(kConfigParam.Get(), fieldtrial_config);
    base::UmaHistogramBoolean(kConfigParamParseSuccessHistogram, parsed);
    if (!parsed) {
      return default_config;
    }
  }

  // Merge the fieldtrial config into the default config.
  //
  // Note: The `MergeFrom()` method will append repeated fields from
  // `fieldtrial_config` to `default_config`. Since the intent is to override
  // the values of repeated fields in `default_config` with the values from
  // `fieldtrial_config`, the repeated fields in `default_config` must be
  // cleared before calling `MergeFrom()` iff the repeated fields have been set
  // in `fieldtrial_config`.
  default_config.MergeFrom(fieldtrial_config);
  return default_config;
}

}  // namespace

// If enabled, the Compose entrypoint will appear in the NTP Searchbox.
BASE_FEATURE(kNtpSearchboxComposeEntrypoint,
             "NtpSearchboxComposeEntrypoint",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsNtpSearchboxComposeEntrypointEnabled() {
  return base::FeatureList::IsEnabled(kNtpSearchboxComposeEntrypoint) ||
         FeatureConfig::Get().enabled;
}

// If enabled, the Composebox will appear upon clicking the NTP Compose
// entrypoint and will be configured based on the supplied configuration param.
BASE_FEATURE(kNtpComposebox,
             "NtpComposebox",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kConfigParam(&kNtpComposebox,
                                                   "ConfigParam",
                                                   "");

const base::FeatureParam<size_t> kDownscaleMaxImageSizeParam(
    &kNtpComposebox,
    "DownscaleMaxImageSizeParam",
    1500000);

const base::FeatureParam<size_t> kDownscaleMaxImageWidthParam(
    &kNtpComposebox,
    "DownscaleMaxImageWidthParam",
    1600);

const base::FeatureParam<size_t> kDownscaleMaxImageHeightParam(
    &kNtpComposebox,
    "DownscaleMaxImageHeightParam",
    1600);

const base::FeatureParam<size_t> ImageCompressionQualityParam(
    &kNtpComposebox,
    "NtpComposeboxImageCompressionQualityParam",
    40);

FeatureConfig::FeatureConfig()
    : enabled(base::FeatureList::IsEnabled(kNtpComposebox)),
      config(GetNTPComposeboxConfig()),
      downscale_max_image_size(kDownscaleMaxImageSizeParam.Get()),
      downscale_max_image_width(kDownscaleMaxImageWidthParam.Get()),
      downscale_max_image_height(kDownscaleMaxImageHeightParam.Get()),
      image_compression_quality(ImageCompressionQualityParam.Get()) {}

}  // namespace ntp_composebox_fieldtrial
