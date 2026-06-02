// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_protection/features.h"

namespace enterprise_data_protection {

BASE_FEATURE(kEnableDeepScanVerdictCacheSize,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(size_t,
                   kVerdictCacheMaxSize,
                   &kEnableDeepScanVerdictCacheSize,
                   /*name=*/"verdict_cache_max_size",
                   /*default_value=*/200);

BASE_FEATURE_PARAM(bool,
                   kVerdictCacheRemoveQueryParams,
                   &kEnableDeepScanVerdictCacheSize,
                   /*name=*/"remove_query_params",
                   /*default_value=*/false);

}  // namespace enterprise_data_protection
