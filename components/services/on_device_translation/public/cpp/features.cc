// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/on_device_translation/public/cpp/features.h"

#include "base/command_line.h"
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

const base::FeatureParam<bool> kTranslationAPIAcceptLanguagesCheck{
    &blink::features::kEnableTranslationAPI,
    "TranslationAPIAcceptLanguagesCheck", true};

const base::FeatureParam<bool> kTranslationAPILimitLanguagePackCount{
    &blink::features::kEnableTranslationAPI,
    "TranslationAPILimitLanguagePackCount", true};

// static
base::FilePath GetTranslateKitBinaryPathFromCommandLine() {
  return GetPathFromCommandLine(kTranslateKitBinaryPath);
}

}  // namespace on_device_translation
