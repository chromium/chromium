// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/public/url_visit_util.h"

#include <math.h>

#include <array>
#include <optional>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/types/processed_value.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_deduplication/deduplication_strategy.h"
#include "components/url_deduplication/docs_url_strip_handler.h"
#include "components/url_deduplication/url_deduplication_helper.h"
#include "components/url_deduplication/url_strip_handler.h"
#include "components/visited_url_ranking/public/features.h"
#include "components/visited_url_ranking/public/fetch_result.h"
#include "components/visited_url_ranking/public/url_visit_schema.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "url/gurl.h"

using segmentation_platform::InputContext;
using segmentation_platform::processing::ProcessedValue;

namespace visited_url_ranking {

namespace {

// Bucketize the value to exponential buckets. Returns lower bound of the
// bucket.
float BucketizeExp(int64_t value, int max_buckets) {
  if (value <= 0) {
    return 0;
  }
  int log_val = floor(log2(value));
  if (log_val >= max_buckets) {
    log_val = max_buckets;
  }
  return pow(2, log_val);
}

// Used as input to the model, it is ok to group some platforms together. Only
// lists platforms that use the service.
enum class PlatformType {
  kUnknown = 0,
  kWindows = 1,
  kLinux = 2,
  kMac = 3,
  kIos = 4,
  kAndroid = 5,
  kOther = 6
};

PlatformType GetPlatformInput() {
#if BUILDFLAG(IS_WIN)
  return PlatformType::kWindows;
#elif BUILDFLAG(IS_MAC)
  return PlatformType::kMac;
#elif BUILDFLAG(IS_LINUX)
  return PlatformType::kLinux;
#elif BUILDFLAG(IS_IOS)
  return PlatformType::kIos;
#elif BUILDFLAG(IS_ANDROID)
  return PlatformType::kAndroid;
#else
  return PlatformType::kOther;
#endif
}

int GetPriority(DecorationType type) {
  switch (type) {
    case DecorationType::kMostRecent:
      return 3;
    case DecorationType::kFrequentlyVisitedAtTime:
      return 2;
    case DecorationType::kFrequentlyVisited:
      return 2;
    case DecorationType::kVisitedXAgo:
      return 1;
    case DecorationType::kUnknown:
      return 0;
  }
}

// Returns a time like "1 hour ago", "2 days ago", etc for the given `time`.
std::u16string FormatRelativeTime(const base::Time& time) {
  base::Time now = base::Time::Now();
  // TimeFormat does not support negative TimeDelta values, so then we use 0.
  return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_ELAPSED,
                                ui::TimeFormat::LENGTH_SHORT,
                                now < time ? base::TimeDelta() : now - time);
}

}  // namespace

std::unique_ptr<url_deduplication::URLDeduplicationHelper>
CreateDefaultURLDeduplicationHelper() {
  std::vector<std::unique_ptr<url_deduplication::URLStripHandler>> handlers;
  url_deduplication::DeduplicationStrategy strategy =
      url_deduplication::DeduplicationStrategy();
  if (features::kVisitedURLRankingDeduplicationDocs.Get()) {
    handlers.push_back(
        std::make_unique<url_deduplication::DocsURLStripHandler>());
  }

  if (features::kVisitedURLRankingDeduplicationFallback.Get()) {
    strategy.clear_query = true;
  }

  if (features::kVisitedURLRankingDeduplicationUpdateScheme.Get()) {
    strategy.update_scheme = true;
  }

  if (features::kVisitedURLRankingDeduplicationClearPath.Get()) {
    strategy.clear_path = true;
  }

  if (features::kVisitedURLRankingDeduplicationIncludeTitle.Get()) {
    strategy.include_title = true;
  }

  auto prefix_list = base::SplitString(
      features::kVisitedURLRankingDeduplicationExcludedPrefixes.Get(), ",:;",
      base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
  strategy.excluded_prefixes = prefix_list;

  strategy.clear_username = true;
  strategy.clear_password = true;
  strategy.clear_ref = true;
  strategy.clear_port = true;

  return std::make_unique<url_deduplication::URLDeduplicationHelper>(
      std::move(handlers), strategy);
}

URLMergeKey ComputeURLMergeKey(
    const GURL& url,
    const std::u16string& title,
    url_deduplication::URLDeduplicationHelper* deduplication_helper) {
  if (deduplication_helper) {
    auto key = deduplication_helper->ComputeURLDeduplicationKey(
        url, base::UTF16ToUTF8(title));
    DCHECK(!key.empty());
    return key;
  }

  // Default to using the original URL as the URL deduplication / merge key.
  return url.spec();
}

scoped_refptr<InputContext> AsInputContext(
    const std::array<FieldSchema, kNumInputs>& fields_schema,
    const URLVisitAggregate& url_visit_aggregate) {
  base::flat_map<std::string, ProcessedValue> signal_value_map;
  signal_value_map.emplace(
      "title", ProcessedValue(base::UTF16ToUTF8(
                   *url_visit_aggregate.GetAssociatedTitles().begin())));
  signal_value_map.emplace(
      "url", ProcessedValue(*url_visit_aggregate.GetAssociatedURLs().begin()));
  signal_value_map.emplace("url_key",
                           ProcessedValue(url_visit_aggregate.url_key));

  auto* local_tab_data =
      (url_visit_aggregate.fetcher_data_map.find(Fetcher::kTabModel) !=
       url_visit_aggregate.fetcher_data_map.end())
          ? std::get_if<URLVisitAggregate::TabData>(
                &url_visit_aggregate.fetcher_data_map.at(Fetcher::kTabModel))
          : nullptr;

  auto* session_tab_data =
      (url_visit_aggregate.fetcher_data_map.find(Fetcher::kSession) !=
       url_visit_aggregate.fetcher_data_map.end())
          ? std::get_if<URLVisitAggregate::TabData>(
                &url_visit_aggregate.fetcher_data_map.at(Fetcher::kSession))
          : nullptr;

  // For cases where distinguishing between local and session tab data is not
  // relevant, prioritize using local tab data when constructing signals.
  auto* tab_data = local_tab_data ? local_tab_data : session_tab_data;

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
      case kTimeSinceLastModifiedSec: {
        base::TimeDelta time_since_last_modified =
            base::Time::Now() - url_visit_aggregate.GetLastVisitTime();
        value = ProcessedValue::FromFloat(time_since_last_modified.InSeconds());
      } break;
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
      case kLocalTabCount:
        if (local_tab_data) {
          value = ProcessedValue::FromFloat(
              BucketizeExp(local_tab_data->tab_count, 50));
        }
        break;
      case kSessionTabCount:
        if (session_tab_data) {
          value = ProcessedValue::FromFloat(
              BucketizeExp(session_tab_data->tab_count, 50));
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
        if (history_data) {
          value =
              ProcessedValue::FromFloat(history_data->last_app_id.has_value());
        }
        break;
      case kPlatform:
        value =
            ProcessedValue::FromFloat(static_cast<float>(GetPlatformInput()));
        break;
      case kSeenCountLastDay:
      case kActivatedCountLastDay:
      case kDismissedCountLastDay:
      case kSeenCountLast7Days:
      case kActivatedCountLast7Days:
      case kDismissedCountLast7Days:
      case kSeenCountLast30Days:
      case kActivatedCountLast30Days:
      case kDismissedCountLast30Days:
        if (url_visit_aggregate.metrics_signals.find(field_schema.name) !=
            url_visit_aggregate.metrics_signals.end()) {
          value = ProcessedValue::FromFloat(
              url_visit_aggregate.metrics_signals.at(field_schema.name));
        }
        break;
      case kSameTimeGroupVisitCount:
        if (history_data) {
          value = ProcessedValue::FromFloat(
              history_data->same_time_group_visit_count);
        }
        break;
      case kSameDayGroupVisitCount:
        if (history_data) {
          value = ProcessedValue::FromFloat(
              history_data->same_day_group_visit_count);
        }
        break;
    }

    signal_value_map.emplace(field_schema.name, std::move(value));
  }

  scoped_refptr<InputContext> input_context =
      base::MakeRefCounted<InputContext>();
  input_context->metadata_args = std::move(signal_value_map);
  return input_context;
}

const URLVisitAggregate::TabData* GetTabDataIfExists(
    const URLVisitAggregate& url_visit_aggregate) {
  const auto& fetcher_data_map = url_visit_aggregate.fetcher_data_map;
  for (const auto& fetcher : {Fetcher::kTabModel, Fetcher::kSession}) {
    auto it = fetcher_data_map.find(fetcher);
    if (it != fetcher_data_map.end()) {
      const URLVisitAggregate::TabData* tab_data =
          std::get_if<URLVisitAggregate::TabData>(&it->second);
      return tab_data;
    }
  }

  return nullptr;
}

const URLVisitAggregate::Tab* GetTabIfExists(
    const URLVisitAggregate& url_visit_aggregate) {
  const auto& fetcher_data_map = url_visit_aggregate.fetcher_data_map;
  if (fetcher_data_map.find(Fetcher::kSession) != fetcher_data_map.end()) {
    const URLVisitAggregate::TabData* tab_data =
        std::get_if<URLVisitAggregate::TabData>(
            &fetcher_data_map.at(Fetcher::kSession));
    if (tab_data) {
      return &tab_data->last_active_tab;
    }
  }

  if (fetcher_data_map.find(Fetcher::kTabModel) != fetcher_data_map.end()) {
    const URLVisitAggregate::TabData* tab_data =
        std::get_if<URLVisitAggregate::TabData>(
            &fetcher_data_map.at(Fetcher::kTabModel));
    if (tab_data) {
      return &tab_data->last_active_tab;
    }
  }

  return nullptr;
}

const URLVisitAggregate::HistoryData* GetHistoryDataIfExists(
    const URLVisitAggregate& url_visit_aggregate) {
  const auto& fetcher_data_map = url_visit_aggregate.fetcher_data_map;
  auto it = fetcher_data_map.find(Fetcher::kHistory);
  if (it != fetcher_data_map.end()) {
    const URLVisitAggregate::HistoryData* history_data =
        std::get_if<URLVisitAggregate::HistoryData>(&it->second);
    return history_data;
  }

  return nullptr;
}

const history::AnnotatedVisit* GetHistoryEntryVisitIfExists(
    const URLVisitAggregate& url_visit_aggregate) {
  const auto& fetcher_data_map = url_visit_aggregate.fetcher_data_map;
  if (fetcher_data_map.find(Fetcher::kHistory) != fetcher_data_map.end()) {
    const URLVisitAggregate::HistoryData* history_data =
        std::get_if<URLVisitAggregate::HistoryData>(
            &fetcher_data_map.at(Fetcher::kHistory));
    if (history_data) {
      return &history_data->last_visited;
    }
  }

  return nullptr;
}

const Decoration& GetMostRelevantDecoration(
    const URLVisitAggregate& url_visit_aggregate) {
  const Decoration* result;
  int max_priority = -1;
  for (const auto& decoration : url_visit_aggregate.decorations) {
    if (GetPriority(decoration.GetType()) > max_priority) {
      result = &decoration;
      max_priority = GetPriority(decoration.GetType());
    }
  }
  return *result;
}

std::u16string GetStringForDecoration(DecorationType type,
                                      bool visited_recently) {
#if BUILDFLAG(IS_IOS)
  switch (type) {
    case DecorationType::kMostRecent:
      return l10n_util::GetStringUTF16(
          IDS_TAB_RESUME_DECORATORS_MOST_RECENT_IOS);
    case DecorationType::kFrequentlyVisitedAtTime:
      return l10n_util::GetStringUTF16(
          IDS_TAB_RESUME_DECORATORS_FREQUENTLY_VISITED_IOS);
    case DecorationType::kFrequentlyVisited:
      return l10n_util::GetStringUTF16(
          IDS_TAB_RESUME_DECORATORS_FREQUENTLY_VISITED_IOS);
    case DecorationType::kVisitedXAgo:
      if (visited_recently) {
        return l10n_util::GetStringUTF16(
            IDS_TAB_RESUME_DECORATORS_VISITED_RECENTLY_IOS);
      } else {
        return l10n_util::GetStringUTF16(
            IDS_TAB_RESUME_DECORATORS_VISITED_X_AGO_IOS);
      }
    case DecorationType::kUnknown:
      if (visited_recently) {
        return l10n_util::GetStringUTF16(
            IDS_TAB_RESUME_DECORATORS_VISITED_RECENTLY_IOS);
      } else {
        return l10n_util::GetStringUTF16(
            IDS_TAB_RESUME_DECORATORS_VISITED_X_AGO_IOS);
      }
  }
#else
  switch (type) {
    case DecorationType::kMostRecent:
      return l10n_util::GetStringUTF16(IDS_TAB_RESUME_DECORATORS_MOST_RECENT);
    case DecorationType::kFrequentlyVisitedAtTime:
      return l10n_util::GetStringUTF16(
          IDS_TAB_RESUME_DECORATORS_FREQUENTLY_VISITED);
    case DecorationType::kFrequentlyVisited:
      return l10n_util::GetStringUTF16(
          IDS_TAB_RESUME_DECORATORS_FREQUENTLY_VISITED);
    case DecorationType::kVisitedXAgo:
      if (visited_recently) {
        return l10n_util::GetStringUTF16(
            IDS_TAB_RESUME_DECORATORS_VISITED_RECENTLY);
      } else {
        return l10n_util::GetStringUTF16(
            IDS_TAB_RESUME_DECORATORS_VISITED_X_AGO);
      }
    case DecorationType::kUnknown:
      if (visited_recently) {
        return l10n_util::GetStringUTF16(
            IDS_TAB_RESUME_DECORATORS_VISITED_RECENTLY);
      } else {
        return l10n_util::GetStringUTF16(
            IDS_TAB_RESUME_DECORATORS_VISITED_X_AGO);
      }
  }
#endif
}

std::u16string GetStringForRecencyDecorationWithTime(
    base::Time last_visit_time,
    base::TimeDelta recently_visited_minutes_threshold) {
  if (base::Time::Now() - last_visit_time <
      recently_visited_minutes_threshold) {
    return GetStringForDecoration(DecorationType::kVisitedXAgo,
                                  /*visited_recently=*/true);
  }

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  std::u16string relative_time = FormatRelativeTime(last_visit_time);
  if (relative_time.find(u"hour") != std::string::npos) {
    relative_time.erase(relative_time.find(u"hour"));
    relative_time +=
        l10n_util::GetStringUTF16(IDS_TAB_RESUME_N_HOURS_AGO_NARROW);
  } else if (relative_time.find(u"min") != std::string::npos) {
    relative_time.erase(relative_time.find(u"min"));
    relative_time +=
        l10n_util::GetStringUTF16(IDS_TAB_RESUME_N_MINUTES_AGO_NARROW);
  }
  return GetStringForDecoration(DecorationType::kVisitedXAgo) + u" " +
         relative_time;
#else
  return GetStringForDecoration(DecorationType::kVisitedXAgo) + u" " +
         FormatRelativeTime(last_visit_time);
#endif
}

}  // namespace visited_url_ranking
