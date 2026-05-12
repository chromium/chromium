// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/soda/soda_features.h"

#include "base/feature_list.h"

namespace speech {
#if BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kFeatureManagementCrosSodaConchLanguages,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif
}  // namespace speech
