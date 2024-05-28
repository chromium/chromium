// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_PUBLIC_FEATURES_H_
#define COMPONENTS_VISITED_URL_RANKING_PUBLIC_FEATURES_H_

#include "base/feature_list.h"

namespace visited_url_ranking::features {

// Core feature flag for Visited URL Ranking service.
BASE_DECLARE_FEATURE(kVisitedURLRankingService);

// Parameter determining the fetch option's default query duration in hours.
extern const char kVisitedURLRankingFetchDurationInHoursParam[];

}  // namespace visited_url_ranking::features

#endif  // COMPONENTS_VISITED_URL_RANKING_PUBLIC_FEATURES_H_
