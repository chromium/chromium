// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/public/features.h"

#include "build/build_config.h"

namespace visited_url_ranking::features {

BASE_FEATURE(kVisitedURLRankingService,
             "VisitedURLRankingService",
             base::FEATURE_ENABLED_BY_DEFAULT);

const char kVisitedURLRankingFetchDurationInHoursParam[] =
    "VisitedURLRankingFetchDurationInHoursParam";

}  // namespace visited_url_ranking::features
