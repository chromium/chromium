// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "url/gurl.h"

namespace visited_url_ranking {

using history::VisitContextAnnotations;

history::AnnotatedVisit GenerateSampleAnnotatedVisit(
    history::VisitID visit_id,
    const std::u16string& page_title,
    const GURL& url,
    bool has_url_keyed_image,
    const std::string& originator_cache_guid,
    float visibility_score,
    const std::vector<history::VisitContentModelAnnotations::Category>&
        categories,
    const base::Time visit_time,
    const VisitContextAnnotations::BrowserType browser_type) {
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

  if (browser_type != VisitContextAnnotations::BrowserType::kUnknown) {
    auto context_annotations = history::VisitContextAnnotations();
    context_annotations.on_visit.browser_type = browser_type;
    annotated_visit.context_annotations = std::move(context_annotations);
  }

  return annotated_visit;
}

URLVisitAggregate CreateSampleURLVisitAggregate(const GURL& url,
                                                float visibility_score,
                                                base::Time time,
                                                std::set<Fetcher> fetchers) {
  URLVisitAggregate visit_aggregate(url.spec());
  visit_aggregate.bookmarked = true;
  visit_aggregate.decorations.emplace_back(DecorationType::kVisitedXAgo,
                                           u"You visited X ago");

  const std::u16string kSampleTitle(u"sample_title");
  if (fetchers.contains(Fetcher::kHistory)) {
    visit_aggregate.fetcher_data_map.emplace(
        Fetcher::kHistory,
        URLVisitAggregate::HistoryData(GenerateSampleAnnotatedVisit(
            1, kSampleTitle, url, true, "foreign_session_guid",
            visibility_score, {}, time,
            VisitContextAnnotations::BrowserType::kUnknown)));
  }
  if (fetchers.contains(Fetcher::kSession)) {
    auto tab_data = URLVisitAggregate::TabData(URLVisitAggregate::Tab(
        1,
        URLVisit(url, kSampleTitle, time,
                 syncer::DeviceInfo::FormFactor::kUnknown,
                 URLVisit::Source::kLocal),
        "sample_tag", "sample_session_name"));
    tab_data.last_active = time;
    visit_aggregate.fetcher_data_map.emplace(Fetcher::kSession,
                                             std::move(tab_data));
  }
  if (fetchers.contains(Fetcher::kTabModel)) {
    auto tab_data = URLVisitAggregate::TabData(URLVisitAggregate::Tab(
        1, URLVisit(url, kSampleTitle, time,
                    syncer::DeviceInfo::FormFactor::kUnknown,
                    URLVisit::Source::kLocal)));
    tab_data.last_active = time;
    visit_aggregate.fetcher_data_map.emplace(Fetcher::kTabModel,
                                             std::move(tab_data));
  }

  return visit_aggregate;
}

}  // namespace visited_url_ranking
