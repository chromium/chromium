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

// The free disk space in MiB that a language pack download needs. When the
// component updater volume has less, `Translator.create()` fails without
// starting the download. 0 turns the check off.
extern const base::FeatureParam<size_t>
    kTranslationAPILanguagePackMinFreeDiskSpaceMB;

// How long a translator in the translation service can go without a
// translation before the service unloads its TranslateKit model, and whether
// moderate or critical memory pressure unloads all models immediately. The
// model loads again on the next translation. 0 keeps the model loaded for as
// long as the translator exists.
extern const base::FeatureParam<base::TimeDelta>
    kTranslationAPIModelIdleUnloadTimeout;

// The largest input in UTF-8 bytes that `translate()` and
// `translateStreaming()` accept. Larger input fails with a QuotaExceededError
// before it reaches the translation service. 0 or less turns the limit off.
// The limit applies to each call. It doesn't limit how many calls a page
// queues, which the service runs one after another.
extern const base::FeatureParam<int> kTranslationAPIMaxInputSizeBytes;

// Minimum total physical RAM in MB required to use on-device translation.
// Devices below this floor report the API as unavailable. 0 disables the check.
extern const base::FeatureParam<size_t> kTranslationAPIMinPhysicalMemoryMB;

// The maximum number of language packs that may be installed or registered at
// once. A request that needs more missing packs fails before starting any
// download. 0 or less disables the cap.
extern const base::FeatureParam<int> kTranslationAPIMaxInstalledLanguagePacks;

// Blocks TranslateKit library and language pack downloads and updates on
// metered network connections when enabled.
BASE_DECLARE_FEATURE(kTranslationAPIBlockDownloadsOnMeteredNetwork);

// Enables automatic download of language packs for on-device translation.
BASE_DECLARE_FEATURE(kAutoDownloadTranslateLanguagePacks);

// The list of language pairs for which language packs should be automatically
// downloaded. The format is a comma-separated list of language pairs, e.g.
// "en-es,en-fr".
extern const base::FeatureParam<std::string>
    kAutoDownloadTranslateLanguagePacksLanguagePairs;

// Enables loading and using V3 TranslateKit language packs.
BASE_DECLARE_FEATURE(kTranslateKitV3Models);

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
