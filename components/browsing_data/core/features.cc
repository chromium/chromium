// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/features.h"

#include "build/build_config.h"

namespace browsing_data::features {
#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kBrowsingDataModel,
             "BrowsingDataModel",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)
}  // namespace browsing_data::features
