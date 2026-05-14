// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/common/translate_features.h"

namespace translate {

BASE_FEATURE(kEnableTranslatePdf, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTranslateSimplifiedHindi, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTranslateLanguageSearchUI, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTranslateElementExperimentFeatures,
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kTranslateElementExperimentFeaturesParam{
    &kTranslateElementExperimentFeatures, "ef", ""};

}  // namespace translate
