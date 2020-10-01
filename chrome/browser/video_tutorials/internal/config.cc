// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/internal/config.h"

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/video_tutorials/switches.h"

namespace video_tutorials {

// Default base URL string for the server.
constexpr char kDefaultBaseURL[] = "https://chromeupboarding-pa.googleapis.com";

// Default URL string for GetTutorials RPC.
constexpr char kDefaultGetTutorialsPath[] = "/v1/videotutorials";

// Hindi is the default locale.
constexpr char kDefaultPreferredLocale[] = "hi";

// Finch parameter key for base server URL to retrieve the tutorials.
constexpr char kBaseURLKey[] = "base_url";

constexpr char kPreferredLocaleConfigKey[] = "default_locale";

constexpr char kFetchFrequencyKey[] = "fetch_frequency";

constexpr char kExperimentTagKey[] = "experiment_tag";

// Default frequency in days for fetching tutorial metatada from server.
constexpr int kDefaultFetchFrequencyDays = 15;

namespace {
const GURL BuildGetTutorialsEndpoint(const GURL& base_url, const char* path) {
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  return base_url.ReplaceComponents(replacements);
}

}  // namespace

// static
GURL Config::GetTutorialsServerURL() {
  std::string base_url_from_finch = base::GetFieldTrialParamValueByFeature(
      features::kVideoTutorials, kBaseURLKey);
  GURL server_url = base_url_from_finch.empty() ? GURL(kDefaultBaseURL)
                                                : GURL(base_url_from_finch);
  return BuildGetTutorialsEndpoint(server_url, kDefaultGetTutorialsPath);
}

// static
std::string Config::GetDefaultPreferredLocale() {
  std::string default_locale_from_finch =
      base::GetFieldTrialParamValueByFeature(features::kVideoTutorials,
                                             kPreferredLocaleConfigKey);
  return default_locale_from_finch.empty() ? kDefaultPreferredLocale
                                           : default_locale_from_finch;
}

// static
base::TimeDelta Config::GetFetchFrequency() {
  int frequency_in_days = base::GetFieldTrialParamByFeatureAsInt(
      features::kVideoTutorials, kFetchFrequencyKey,
      kDefaultFetchFrequencyDays);
  return base::TimeDelta::FromDays(frequency_in_days);
}

// static
std::string Config::GetExperimentTag() {
  return base::GetFieldTrialParamValueByFeature(features::kVideoTutorials,
                                                kExperimentTagKey);
}

}  // namespace video_tutorials
