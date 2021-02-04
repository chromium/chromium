// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_INFO_FEATURES_H_
#define COMPONENTS_PAGE_INFO_FEATURES_H_

#include "build/build_config.h"

namespace base {
struct Feature;
}  // namespace base

namespace page_info {

#if defined(OS_ANDROID)
// Enables the discoverability ui animations for Page Info.
extern const base::Feature kPageInfoDiscoverability;
// Enables the history sub page for Page Info.
extern const base::Feature kPageInfoHistory;
// Enables the second version of the Page Info View.
extern const base::Feature kPageInfoV2;
#endif

}  // namespace page_info

#endif  // COMPONENTS_PAGE_INFO_ANDROID_FEATURES_H_