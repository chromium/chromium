// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/soda/soda_features.h"

#include "base/feature_list.h"

namespace speech {
#if BUILDFLAG(IS_CHROMEOS_ASH)
BASE_FEATURE(kCrosExpandSodaLanguages,
             "CrosExpandSodaLanguages",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kFeatureManagementCrosSodaConchLanguages,
             "FeatureManagementCrosSodaConchLanguages",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCrosSodaConchLanguages,
             "CrosSodaConchLanguages",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif
}  // namespace speech
