// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/common_features.h"

namespace subresource_filter {

BASE_FEATURE(kAdTagging, "AdTagging", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTPCDAdHeuristicSubframeRequestTagging,
             "TPCDAdHeuristicSubframeRequestTagging",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace subresource_filter
