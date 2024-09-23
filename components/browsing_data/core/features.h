// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CORE_FEATURES_H_
#define COMPONENTS_BROWSING_DATA_CORE_FEATURES_H_

#include "base/feature_list.h"

namespace browsing_data::features {

// Pipes down the BrowsingDataModel to power site settings on Android.
#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kBrowsingDataModel);
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace browsing_data::features

#endif  // COMPONENTS_BROWSING_DATA_CORE_FEATURES_H_
