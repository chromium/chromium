// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ttc/features.h"

namespace ttc {

BASE_FEATURE(kTtc, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTtcLiveRecipe, base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<TtcLiveRecipe>::Option kTtcLiveRecipeOptions[] = {
    {TtcLiveRecipe::kUnspecified, "unspecified"},
    {TtcLiveRecipe::kFallback, "fallback"},
    {TtcLiveRecipe::kDefault, "default"},
};

BASE_FEATURE_ENUM_PARAM(TtcLiveRecipe,
                        kLiveRecipe,
                        &kTtcLiveRecipe,
                        TtcLiveRecipe::kUnspecified,
                        &kTtcLiveRecipeOptions);

BASE_FEATURE_PARAM(base::TimeDelta, kTtcSetupTimeout, &kTtc, base::Seconds(20));

}  // namespace ttc
