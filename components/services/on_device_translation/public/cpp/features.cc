// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/on_device_translation/public/cpp/features.h"

#include "base/command_line.h"
#include "third_party/blink/public/common/features_generated.h"

namespace on_device_translation {

namespace {

// Limit the number of downloadable language packs to 3 during OT to mitigate
// the risk of fingerprinting attacks.
constexpr size_t kTranslationAPILimitLanguagePackCountMax = 3;

base::FilePath GetPathFromCommandLine(const char* switch_name) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switch_name)) {
    return base::FilePath();
  }
  return command_line->GetSwitchValuePath(switch_name);
}

}  // namespace

const base::FeatureParam<bool> kTranslationAPIAcceptLanguagesCheck{
    &blink::features::kTranslationAPI, "TranslationAPIAcceptLanguagesCheck",
    true};

const base::FeatureParam<bool> kTranslationAPILimitLanguagePackCount{
    &blink::features::kTranslationAPI, "TranslationAPILimitLanguagePackCount",
    true};

const base::FeatureParam<base::TimeDelta> kTranslationAPIServiceIdleTimeout{
    &blink::features::kTranslationAPI, "TranslationAPIServiceIdleTimeout",
    base::Minutes(1)};

const base::FeatureParam<size_t> kTranslationAPIMaxServiceCount{
    &blink::features::kTranslationAPI, "TranslationAPIMaxServiceCount", 10};

// static
base::FilePath GetTranslateKitBinaryPathFromCommandLine() {
  return GetPathFromCommandLine(kTranslateKitBinaryPath);
}

size_t GetInstallablePackageCount(size_t installed_package_count) {
  if (!kTranslationAPILimitLanguagePackCount.Get()) {
    return std::numeric_limits<size_t>::max();
  }
  if (installed_package_count >= kTranslationAPILimitLanguagePackCountMax) {
    return 0;
  }
  return kTranslationAPILimitLanguagePackCountMax - installed_package_count;
}

}  // namespace on_device_translation
