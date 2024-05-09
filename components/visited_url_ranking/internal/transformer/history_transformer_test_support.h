// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_INTERNAL_TRANSFORMER_HISTORY_TRANSFORMER_TEST_SUPPORT_H_
#define COMPONENTS_VISITED_URL_RANKING_INTERNAL_TRANSFORMER_HISTORY_TRANSFORMER_TEST_SUPPORT_H_

#include <vector>

#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "url/gurl.h"

namespace visited_url_ranking {

history::AnnotatedVisit GenerateSampleAnnotatedVisit(
    history::VisitID visit_id,
    const std::u16string& page_title,
    const GURL& url,
    bool has_url_keyed_image,
    const std::string& originator_cache_guid = "",
    float visibility_score = 1.0f,
    const std::vector<history::VisitContentModelAnnotations::Category>&
        categories = {},
    const base::Time visit_time = base::Time::Now());

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_INTERNAL_TRANSFORMER_HISTORY_TRANSFORMER_TEST_SUPPORT_H_
