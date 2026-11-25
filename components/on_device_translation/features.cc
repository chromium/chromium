// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/features.h"

#include <cstddef>
#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/version.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/features_generated.h"

namespace on_device_translation {

namespace {

base::FilePath GetPathFromCommandLine(const char* switch_name) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switch_name)) {
    return base::FilePath();
  }
  return command_line->GetSwitchValuePath(switch_name);
}

}  // namespace

const base::FeatureParam<std::string> kTranslationAPILibraryMinimumVersion{
    &blink::features::kTranslationAPI, "TranslationAPILibraryMinimumVersion",
    "2025.1.10.0"};

const base::FeatureParam<base::TimeDelta> kTranslationAPIServiceIdleTimeout{
    &blink::features::kTranslationAPI, "TranslationAPIServiceIdleTimeout",
    base::Minutes(1)};

const base::FeatureParam<size_t> kTranslationAPIMaxServiceCount{
    &blink::features::kTranslationAPI, "TranslationAPIMaxServiceCount",
#if BUILDFLAG(IS_ANDROID)
    // Each service is a separate sandboxed process that holds about 79 to 84
    // MiB per loaded language pair.
    2
#else
    10
#endif  // BUILDFLAG(IS_ANDROID)
};

// Require 5 GiB of free disk space by default before starting a new language
// pack download across all platforms.
const base::FeatureParam<size_t> kTranslationAPILanguagePackMinFreeDiskSpaceMB{
    &blink::features::kTranslationAPI,
    "TranslationAPILanguagePackMinFreeDiskSpaceMB", 5120};

const base::FeatureParam<base::TimeDelta> kTranslationAPIModelIdleUnloadTimeout{
    &blink::features::kTranslationAPI, "TranslationAPIModelIdleUnloadTimeout",
#if BUILDFLAG(IS_ANDROID)
    // A page can keep a translator for its whole lifetime, holding about 79 to
    // 84 MiB per loaded language pair in the service process. Unload idle
    // models after 1 minute to avoid holding that memory indefinitely.
    base::Minutes(1)
#else
    base::TimeDelta()
#endif  // BUILDFLAG(IS_ANDROID)
};

const base::FeatureParam<int> kTranslationAPIMaxInputSizeBytes{
    &blink::features::kTranslationAPI, "TranslationAPIMaxInputSizeBytes",
#if BUILDFLAG(IS_ANDROID)
    // The engine translates the whole input of one call at once, in a process
    // that phones may run short of memory for. 256 KiB is about 40,000
    // English words.
    256 * 1024
#else
    0
#endif  // BUILDFLAG(IS_ANDROID)
};

const base::FeatureParam<size_t> kTranslationAPIMinPhysicalMemoryMB{
    &blink::features::kTranslationAPI, "TranslationAPIMinPhysicalMemoryMB",
#if BUILDFLAG(IS_ANDROID)
    // Leaves out devices with 2 GB of RAM or less, which report less than
    // 2048 MB. That includes Android Go devices.
    2048
#else
    0
#endif  // BUILDFLAG(IS_ANDROID)
};

const base::FeatureParam<int> kTranslationAPIMaxInstalledLanguagePacks{
    &blink::features::kTranslationAPI,
    "TranslationAPIMaxInstalledLanguagePacks",
#if BUILDFLAG(IS_ANDROID)
    // Each unpacked language pack uses about 80 MiB on disk.
    3
#else
    0
#endif  // BUILDFLAG(IS_ANDROID)
};

BASE_FEATURE(kTranslationAPIBlockDownloadsOnMeteredNetwork,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAutoDownloadTranslateLanguagePacks,
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string>
    kAutoDownloadTranslateLanguagePacksLanguagePairs{
        &kAutoDownloadTranslateLanguagePacks, "language_pairs", ""};

BASE_FEATURE(kTranslateKitV3Models, base::FEATURE_DISABLED_BY_DEFAULT);

// static
base::FilePath GetTranslateKitBinaryPathFromCommandLine() {
  return GetPathFromCommandLine(kTranslateKitBinaryPath);
}

bool IsValidTranslateKitVersion(std::string_view version_str) {
  base::Version minimum_version(kTranslationAPILibraryMinimumVersion.Get());
  CHECK(minimum_version.IsValid());

  base::Version version(version_str);
  if (!version.IsValid()) {
    return false;
  }
  if (version.components().size() != minimum_version.components().size()) {
    return false;
  }

  return version.CompareTo(minimum_version) >= 0;
}

}  // namespace on_device_translation
