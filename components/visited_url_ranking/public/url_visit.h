// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_PUBLIC_URL_VISIT_H_
#define COMPONENTS_VISITED_URL_RANKING_PUBLIC_URL_VISIT_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <variant>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "components/segmentation_platform/public/trigger.h"
#include "components/sync_device_info/device_info.h"
#include "components/visited_url_ranking/public/decoration.h"
#include "url/gurl.h"

namespace visited_url_ranking {

// The currently supported URL visit data fetchers that may participate in a
// fetch request.
enum class Fetcher {
  kTabModel = 0,
  kSession = 1,
  kHistory = 2,
};

// URL visit associated data.
struct URLVisit {
  // If applicable, whether the visit data was sourced from a specific origin
  // (local or remote).
  enum class Source { kNotApplicable = 0, kLocal = 1, kForeign = 2 };

  URLVisit(const GURL& url_arg,
           const std::u16string& title_arg,
           const base::Time& last_modified_arg,
           syncer::DeviceInfo::FormFactor device_type_arg,
           Source source_arg,
           const std::optional<std::string>& client_name = std::nullopt);
  URLVisit(const URLVisit&);
  ~URLVisit();

  // The page URL associated with the data.
  GURL url;
  // The page title of the URL.
  std::u16string title;
  // Timestamp for when the URL was last modified (e.g.: When a page reloads
  // or there is a favicon change).
  base::Time last_modified;
  // The device form factor in which the visit took place.
  syncer::DeviceInfo::FormFactor device_type =
      syncer::DeviceInfo::FormFactor::kUnknown;
  // The source from which the visit originated (i.e. local or remote).
  Source source = Source::kNotApplicable;
  // The visit's user visible client name, if applicable. Only set for remote
  // sources.
  std::optional<std::string> client_name;
};

/**
 * A wrapper data type that encompasses URL visit related data from various
 * sources.
 */
struct URLVisitAggregate {
  // Captures tab data associated with a given URL visit.
  struct Tab {
    Tab(int32_t id_arg,
        URLVisit visit_arg,
        std::optional<std::string> session_tag_arg = std::nullopt,
        std::optional<std::string> session_name_arg = std::nullopt);
    Tab(const Tab&);
    ~Tab();

    // A unique tab identifier.
    int32_t id = -1;
    // Associated URL visit data.
    URLVisit visit;
    // The tab's unique session tag, if applicable.
    std::optional<std::string> session_tag;
    // The tab's user visible session name, if applicable.
    std::optional<std::string> session_name;
  };

  // Captures aggregate tab data associated with a URL visit for a given time
  // period.
  struct TabData {
    explicit TabData(Tab last_active_tab_arg);
    TabData(const TabData&);
    ~TabData();
    // The last active tab associated with a given URL visit.
    Tab last_active_tab;
    // Timestamp for when a tab associated with the given URL visit was last
    // activated.
    base::Time last_active;
    // Whether there is a tab for the given URL visit that is pinned.
    bool pinned = false;
    // Whether there is a tab for the given URL visit that is part of a group.
    bool in_group = false;
    // The number of opened tabs for the given URL visit aggregate in a time
    // period.
    size_t tab_count = 1;
  };

  struct HistoryData {
    explicit HistoryData(history::AnnotatedVisit annotated_visit,
                         std::optional<std::string> client_name = std::nullopt,
                         syncer::DeviceInfo::FormFactor device_type_arg =
                             syncer::DeviceInfo::FormFactor::kUnknown);
    HistoryData(const HistoryData&) = delete;
    HistoryData(HistoryData&& other);
    HistoryData& operator=(HistoryData&& other);
    ~HistoryData();

    // The last annotated visit associated with the given URL visit in a given
    // time period.
    history::AnnotatedVisit last_visited;

    // Associated URL visit data.
    URLVisit visit;

    // The last `app_id` value if any for any of the visits associated with
    // the URL visit aggregate.
    std::optional<std::string> last_app_id = std::nullopt;

    // Whether any of the annotated visits for the given URL visit aggregate are
    // part of a cluster.
    bool in_cluster = false;

    // The total duration in the foreground for all visits associated with the
    // aggregate in a time period.
    base::TimeDelta total_foreground_duration = base::Seconds(0);

    // The number of history visits associated with the URL visit aggregate in a
    // time period.
    size_t visit_count = 1;

    // The number of history visits that took place on the same time group as
    // the current visit. See `url_visit_util.h|cc` for details on the
    // definition of a time group.
    size_t same_time_group_visit_count = 0;

    // The number of history visits that took place on the same day group as the
    // current visit. See `url_visit_util.h|cc` for details on the definition of
    // a day group.
    size_t same_day_group_visit_count = 0;
  };

  explicit URLVisitAggregate(std::string key_arg);
  URLVisitAggregate(const URLVisitAggregate&) = delete;
  URLVisitAggregate(URLVisitAggregate&& other);
  URLVisitAggregate& operator=(URLVisitAggregate&& other);
  ~URLVisitAggregate();

  // A unique identifier that maps to a collection of associated URL visits.
  // Computed via a merging and deduplication strategy and used to record events
  // associated with the URL visit aggregate.
  std::string url_key;

  // An ID used to collect metrics associated with the aggregate visit for model
  // training purposes. See `VisitedURLRankingService::RecordAction` for more
  // details.
  segmentation_platform::TrainingRequestId request_id;

  // Returns a set of associated URL titles present in the data provided by the
  // various fetchers that participated in constructing the aggregate object.
  std::set<std::u16string_view> GetAssociatedTitles() const;

  // Returns a set of associated visit URLs present in the data provided by the
  // various fetchers that participated in constructing the aggregate object.
  std::set<const GURL*> GetAssociatedURLs() const;

  // Utility to fetch timestamp that the URL was last opened on a tab.
  base::Time GetLastVisitTime() const;

  // A map of aggregate tab related characteristics associated with the visit as
  // provided by a given source.
  using URLVisitVariant =
      std::variant<URLVisitAggregate::TabData, URLVisitAggregate::HistoryData>;
  std::map<Fetcher, URLVisitVariant> fetcher_data_map;

  // Whether the visit is bookmarked or not.
  bool bookmarked = false;

  // The number of times the visits associated with the aggregate where on the
  // foreground.
  size_t num_times_active = 0;

  // A map of additional metrics signals intended only for ML use.
  std::map<std::string, float> metrics_signals;

  // A score associated with the aggregate, if any.
  std::optional<float> score = std::nullopt;

  // The matching decorations for a URL visit aggregate. One of these will be
  // selected to display on various UI surfaces.
  std::vector<Decoration> decorations;
};

// Helper to visit each variant of URLVisitVariant.
// Usage:
//   std::visit(URLVisitVariantHelper{
//         [](Variant1& variant1) {},
//         [](Variant2& variant1) {},
//         [](Variant3& variant1) {},
//      variant_data);
template <class... Ts>
struct URLVisitVariantHelper : Ts... {
  using Ts::operator()...;
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_PUBLIC_URL_VISIT_H_
