// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_ai/ai_utils.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

using optimization_guide::OnDeviceError;

namespace on_device_ai {

blink::mojom::ModelStreamingResponseStatus ConvertOnDeviceError(
    OnDeviceError error) {
  switch (error) {
    case OnDeviceError::kInvalidRequest:
      return blink::mojom::ModelStreamingResponseStatus::kErrorInvalidRequest;
    case OnDeviceError::kGenericFailure:
      return blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure;
    case OnDeviceError::kUnsupportedLanguage:
      return blink::mojom::ModelStreamingResponseStatus::
          kErrorUnsupportedLanguage;
    case OnDeviceError::kFiltered:
      return blink::mojom::ModelStreamingResponseStatus::kErrorFiltered;
    case OnDeviceError::kDisabled:
      return blink::mojom::ModelStreamingResponseStatus::kErrorDisabled;
    case OnDeviceError::kCancelled:
      return blink::mojom::ModelStreamingResponseStatus::kErrorCancelled;
    case OnDeviceError::kResponseLowQuality:
      return blink::mojom::ModelStreamingResponseStatus::
          kErrorResponseLowQuality;
  }
}

// Returns nullopt if all languages are enabled.
std::optional<base::flat_set<std::string>> GetEnabledLanguagesForFeature(
    const base::flat_set<std::string>& default_supported,
    const base::FeatureParam<std::string>& feature_param) {
  if (feature_param.Get() == "*") {
    return std::nullopt;
  }

  std::vector<std::string> enabled_languages =
      base::SplitString(feature_param.Get(), ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  std::sort(enabled_languages.begin(), enabled_languages.end());

  std::vector<std::string> difference;
  std::set_difference(enabled_languages.begin(), enabled_languages.end(),
                      default_supported.begin(), default_supported.end(),
                      std::back_inserter(difference));
  if (!difference.empty()) {
    LOG(WARNING) << "Enabled languages (" << base::JoinString(difference, ", ")
                 << ") are not supported for " << feature_param.feature->name;
  }

  base::flat_set<std::string> all_enabled_languages(default_supported.begin(),
                                                    default_supported.end());
  for (const auto& lang : enabled_languages) {
    all_enabled_languages.insert(lang);
  }

  LOG_IF(WARNING, all_enabled_languages.empty())
      << "Supported languages is empty: " << feature_param.feature->name;

  return all_enabled_languages;
}

}  // namespace on_device_ai
