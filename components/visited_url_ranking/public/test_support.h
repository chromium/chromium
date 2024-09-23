// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_PUBLIC_TEST_SUPPORT_H_
#define COMPONENTS_VISITED_URL_RANKING_PUBLIC_TEST_SUPPORT_H_

#include <set>
#include <vector>

#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "url/gurl.h"

namespace visited_url_ranking {

inline constexpr char kSampleSearchUrl[] =
    "https://www.google.com/search?q=sample";

history::AnnotatedVisit GenerateSampleAnnotatedVisit(
    history::VisitID visit_id,
    const std::u16string& page_title,
    const GURL& url,
    bool has_url_keyed_image,
    const std::string& originator_cache_guid = "",
    float visibility_score = 1.0f,
    const std::vector<history::VisitContentModelAnnotations::Category>&
        categories = {},
    const base::Time visit_time = base::Time::Now(),
    const history::VisitContextAnnotations::BrowserType browser_type =
        history::VisitContextAnnotations::BrowserType::kUnknown);

URLVisitAggregate CreateSampleURLVisitAggregate(
    const GURL& url,
    float visibility_score = 1.0f,
    base::Time time = base::Time::Now(),
    std::set<Fetcher> fetchers = {Fetcher::kHistory, Fetcher::kSession,
                                  Fetcher::kTabModel});

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_PUBLIC_TEST_SUPPORT_H_
