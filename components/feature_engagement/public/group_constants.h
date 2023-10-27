// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_GROUP_CONSTANTS_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_GROUP_CONSTANTS_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace feature_engagement {

// A feature to ensure all arrays can contain at least one group.
BASE_DECLARE_FEATURE(kIPHDummyGroup);

#if BUILDFLAG(IS_IOS)
BASE_DECLARE_FEATURE(kiOSFullscreenPromosGroup);
#endif  // BUILDFLAG(IS_IOS)

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_GROUP_CONSTANTS_H_
