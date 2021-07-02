// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "ui/base/ui_base_features.h"

namespace ntp_tiles {

const char kPopularSitesFieldTrialName[] = "NTPPopularSites";

const base::Feature kPopularSitesBakedInContentFeature{
    "NTPPopularSitesBakedInContent", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kNtpMostLikelyFaviconsFromServerFeature{
    "NTPMostLikelyFaviconsFromServer", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kUsePopularSitesSuggestions{
    "UsePopularSitesSuggestions", base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace ntp_tiles
