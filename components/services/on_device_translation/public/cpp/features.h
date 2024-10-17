// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_ON_DEVICE_TRANSLATION_PUBLIC_CPP_FEATURES_H_
#define COMPONENTS_SERVICES_ON_DEVICE_TRANSLATION_PUBLIC_CPP_FEATURES_H_

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/metrics/field_trial_params.h"

namespace on_device_translation {

// When this feature param is enabled, the Translation API will fail if neither
// the source nor destination language is in the AcceptLanguages. This is
// introduced to mitigate privacy concerns.
extern const base::FeatureParam<bool> kTranslationAPIAcceptLanguagesCheck;

// This feature limits the number of language components downloaded by
// createTranslator() to 3.
extern const base::FeatureParam<bool> kTranslationAPILimitLanguagePackCount;

const char kTranslateKitBinaryPath[] = "translate-kit-binary-path";

base::FilePath GetTranslateKitBinaryPathFromCommandLine();

}  // namespace on_device_translation

#endif  // COMPONENTS_SERVICES_ON_DEVICE_TRANSLATION_PUBLIC_CPP_FEATURES_H_
