// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/features.h"

#include "base/feature_list.h"

namespace affiliations::features {

// When enabled, affiliation requests will include a request for group
// affiliation information.
BASE_FEATURE(kAffiliationsGroupInfoEnabled,
             "AffiliationsGroupInfoEnabled",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace affiliations::features
