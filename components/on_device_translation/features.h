// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ON_DEVICE_TRANSLATION_FEATURES_H_
#define COMPONENTS_ON_DEVICE_TRANSLATION_FEATURES_H_

#include <string>
#include <string_view>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace on_device_translation {

// The Translation API will fail the version check if the loaded library version
// is lower than the specified minimum version.
extern const base::FeatureParam<std::string>
    kTranslationAPILibraryMinimumVersion;

// Version string is in the format of "xxxx.xx.xx.xx".
// Any version string longer than this will be truncated.
inline constexpr size_t kTranslationAPILibraryVersionStringSize = 14;

// The duration that the OnDeviceTranslation service can remain idle before it
// is terminated.
extern const base::FeatureParam<base::TimeDelta>
    kTranslationAPIServiceIdleTimeout;

// The maximum number of on device translation service instances that can be
// created per browser context.
extern const base::FeatureParam<size_t> kTranslationAPIMaxServiceCount;

// Enables the translateStreaming API by splitting the input into sentences.
BASE_DECLARE_FEATURE(kTranslateStreamingBySentence);

// Enables automatic download of language packs for on-device translation.
BASE_DECLARE_FEATURE(kAutoDownloadTranslateLanguagePacks);

// The list of language pairs for which language packs should be automatically
// downloaded. The format is a comma-separated list of language pairs, e.g.
// "en-es,en-fr".
extern const base::FeatureParam<std::string>
    kAutoDownloadTranslateLanguagePacksLanguagePairs;

const char kTranslateKitBinaryPath[] = "translate-kit-binary-path";

base::FilePath GetTranslateKitBinaryPathFromCommandLine();

// Checks if the given `version` is valid.
// `version` must be in the same format as the minimum version
// `kTranslationAPILibraryMinimumVersion`.
// Returns false if the format is invalid, or if any of the version components
// is invalid.
bool IsValidTranslateKitVersion(std::string_view version);

}  // namespace on_device_translation

#endif  // COMPONENTS_ON_DEVICE_TRANSLATION_FEATURES_H_
