// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_NTP_COMPOSEBOX_FIELDTRIAL_H_
#define COMPONENTS_SEARCH_NTP_COMPOSEBOX_FIELDTRIAL_H_

#include "base/metrics/field_trial_params.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "third_party/omnibox_proto/ntp_composebox_config.pb.h"

namespace ntp_composebox_fieldtrial {

inline constexpr char kConfigParamParseSuccessHistogram[] =
    "NewTabPage.Composebox.ConfigParseSuccess";

BASE_DECLARE_FEATURE(kNtpSearchboxComposeEntrypoint);

bool IsNtpSearchboxComposeEntrypointEnabled();

BASE_DECLARE_FEATURE(kNtpComposebox);
// The serialized base64 encoded `omnibox::NTPComposeboxConfig`.
extern const base::FeatureParam<std::string> kConfigParam;
// The maximum image size downscaling target (in pixels).
extern const base::FeatureParam<size_t> kDownscaleMaxImageSizeParam;
// The maximum image width downscaling target (in pixels).
extern const base::FeatureParam<size_t> kDownscaleMaxImageWidthParam;
// The maximum image height downscaling target (in pixels).
extern const base::FeatureParam<size_t> kDownscaleMaxImageHeightParam;
// The composition quality to use when encoding images.
extern const base::FeatureParam<size_t> ImageCompressionQualityParam;

struct FeatureConfig : omnibox_feature_configs::Config<FeatureConfig> {
  FeatureConfig();
  // Whether the feature is enabled.
  bool enabled = false;
  // The configuration proto for the feature.
  omnibox::NTPComposeboxConfig config;
  // Maximum image size downscaling target (in pixels).
  int downscale_max_image_size = 0;
  // Maximum image width downscaling target (in pixels).
  int downscale_max_image_width = 0;
  // Maximum image height downscaling target (in pixels).
  int downscale_max_image_height = 0;
  // Composition quality to use when encoding images.
  int image_compression_quality = 0;
};

using ScopedFeatureConfigForTesting =
    omnibox_feature_configs::ScopedConfigForTesting<FeatureConfig>;

}  // namespace ntp_composebox_fieldtrial

#endif  // COMPONENTS_SEARCH_NTP_COMPOSEBOX_FIELDTRIAL_H_
