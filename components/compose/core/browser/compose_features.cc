// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/compose/core/browser/compose_features.h"

namespace compose::features {

BASE_FEATURE(kEnableCompose, "Compose", base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kEnableComposeInputMinWords{&kEnableCompose,
                                                          "input_min_words", 3};
const base::FeatureParam<int> kEnableComposeInputMaxWords{
    &kEnableCompose, "input_max_words", 500};
const base::FeatureParam<int> kEnableComposeInputMaxChars{
    &kEnableCompose, "input_max_chars", 2500};
const base::FeatureParam<int> kEnableComposeInnerTextMaxBytes{
    &kEnableCompose, "inner_text_max_bytes", 1024 * 1024};

BASE_FEATURE(kEnableComposeNudge,
             "ComposeNudge",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableComposeLanguageBypass,
             "ComposeLanguageBypass",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableComposeWebUIAnimations,
             "ComposeWebUIAnimations",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace compose::features
