// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/public/url_visit_util.h"

#include <array>
#include <optional>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/types/processed_value.h"
#include "components/visited_url_ranking/public/fetch_result.h"
#include "components/visited_url_ranking/public/url_visit_schema.h"
#include "url/gurl.h"

using segmentation_platform::InputContext;
using segmentation_platform::processing::ProcessedValue;

namespace visited_url_ranking {

// TODO(crbug.com/335200723): Integrate client configurable merging and
// deduplication logic to produce "merge" keys for provided URLs.
URLMergeKey ComputeURLMergeKey(const GURL& url) {
  return url.spec();
}

scoped_refptr<InputContext> AsInputContext(
    const std::array<FieldSchema, kNumInputs>& fields_schema,
    const URLVisitAggregate& url_visit_aggregate) {
  base::flat_map<std::string, ProcessedValue> signal_value_map;

  auto* tab_data =
      (url_visit_aggregate.fetcher_data_map.find(Fetcher::kSession) !=
       url_visit_aggregate.fetcher_data_map.end())
          ? std::get_if<URLVisitAggregate::TabData>(
                &url_visit_aggregate.fetcher_data_map.at(Fetcher::kSession))
          : nullptr;
  auto* history_data =
      (url_visit_aggregate.fetcher_data_map.find(Fetcher::kHistory) !=
       url_visit_aggregate.fetcher_data_map.end())
          ? std::get_if<URLVisitAggregate::HistoryData>(
                &url_visit_aggregate.fetcher_data_map.at(Fetcher::kHistory))
          : nullptr;

  for (const auto& field_schema : fields_schema) {
    // Initialized to a sentinel value of -1 which represents an undefined
    // value.
    auto value = ProcessedValue::FromFloat(-1);
    switch (field_schema.signal) {
      case kTimeSinceLastModifiedSec:
        if (tab_data) {
          base::TimeDelta time_since_last_modified =
              base::Time::Now() - tab_data->last_active_tab.visit.last_modified;
          value =
              ProcessedValue::FromFloat(time_since_last_modified.InSeconds());
        }
        break;
      case kTimeSinceLastActiveSec:
        if (tab_data && !tab_data->last_active.is_null()) {
          base::TimeDelta time_since_last_active =
              base::Time::Now() - tab_data->last_active;
          value = ProcessedValue::FromFloat(time_since_last_active.InSeconds());
        }
        break;
      case kTimeActiveForTimePeriodSec:
        if (history_data) {
          value = ProcessedValue::FromFloat(
              history_data->total_foreground_duration.InSeconds());
        }
        break;
      case kNumTimesActive:
        value = ProcessedValue::FromFloat(url_visit_aggregate.num_times_active);
        break;
      case kTabCount:
        if (tab_data) {
          value = ProcessedValue::FromFloat(tab_data->tab_count);
        }
        break;
      case kVisitCount:
        if (history_data) {
          value = ProcessedValue::FromFloat(history_data->visit_count);
        }
        break;
      case kIsBookmarked:
        value = ProcessedValue::FromFloat(url_visit_aggregate.bookmarked);
        break;
      case kIsPinned:
        if (tab_data) {
          value = ProcessedValue::FromFloat(tab_data->pinned);
        }
        break;
      case kIsInTabGroup:
        if (tab_data) {
          value = ProcessedValue::FromFloat(tab_data->in_group);
        }
        break;
      case kIsInCluster:
        if (history_data) {
          value = ProcessedValue::FromFloat(history_data->in_cluster);
        }
        break;
      case kHasUrlKeyedImage:
        if (history_data) {
          value = ProcessedValue::FromFloat(
              history_data->last_visited.content_annotations
                  .has_url_keyed_image);
        }
        break;
      case kHasAppId:
        value =
            ProcessedValue::FromFloat(history_data->last_app_id.has_value());
        break;
      default:
        NOTREACHED();
    }

    signal_value_map.emplace(field_schema.name, std::move(value));
  }

  scoped_refptr<InputContext> input_context =
      base::MakeRefCounted<InputContext>();
  input_context->metadata_args = std::move(signal_value_map);
  return input_context;
}

}  // namespace visited_url_ranking
