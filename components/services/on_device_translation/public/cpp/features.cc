// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/on_device_translation/public/cpp/features.h"

#include <cstddef>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/version.h"
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
    &blink::features::kTranslationAPI, "TranslationAPIMaxServiceCount", 10};

BASE_FEATURE(kTranslateStreamingBySentence, base::FEATURE_ENABLED_BY_DEFAULT);

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
