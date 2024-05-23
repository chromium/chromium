// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
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
    const std::string& originator_cache_guid,
    float visibility_score,
    const std::vector<history::VisitContentModelAnnotations::Category>&
        categories,
    const base::Time visit_time) {
  history::URLRow url_row = history::URLRow(url);
  url_row.set_title(page_title);
  history::VisitRow visit_row;
  visit_row.visit_id = visit_id;
  visit_row.visit_time = visit_time;
  visit_row.is_known_to_sync = true;
  visit_row.originator_cache_guid = originator_cache_guid;
  auto model_annotations = history::VisitContentModelAnnotations();
  model_annotations.categories = categories;
  model_annotations.visibility_score = visibility_score;
  auto content_annotations = history::VisitContentAnnotations();
  content_annotations.model_annotations = std::move(model_annotations);
  content_annotations.has_url_keyed_image = has_url_keyed_image;
  history::AnnotatedVisit annotated_visit;
  annotated_visit.url_row = std::move(url_row);
  annotated_visit.visit_row = std::move(visit_row);
  annotated_visit.content_annotations = std::move(content_annotations);

  return annotated_visit;
}

}  // namespace visited_url_ranking
