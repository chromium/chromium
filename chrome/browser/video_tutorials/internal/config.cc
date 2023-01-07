// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/internal/config.h"

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/video_tutorials/switches.h"

namespace video_tutorials {
// Default URL string for GetTutorials RPC.
constexpr char kDefaultGetTutorialsPath[] = "/v1/videotutorials";

// The default locale.
constexpr char kDefaultPreferredLocale[] = "en";

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
GURL Config::GetTutorialsServerURL(const std::string& default_server_url) {
  std::string base_url_from_finch = base::GetFieldTrialParamValueByFeature(
      features::kVideoTutorials, kBaseURLKey);
  if (!base_url_from_finch.empty())
    return BuildGetTutorialsEndpoint(GURL(base_url_from_finch),
                                     kDefaultGetTutorialsPath);
  return default_server_url.empty()
             ? GURL()
             : BuildGetTutorialsEndpoint(GURL(default_server_url),
                                         kDefaultGetTutorialsPath);
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
  return base::Days(frequency_in_days);
}

// static
std::string Config::GetExperimentTag() {
  return base::GetFieldTrialParamValueByFeature(features::kVideoTutorials,
                                                kExperimentTagKey);
}

}  // namespace video_tutorials
