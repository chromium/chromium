// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_PUBLIC_URL_VISIT_H_
#define COMPONENTS_VISITED_URL_RANKING_PUBLIC_URL_VISIT_H_

#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "components/sync_device_info/device_info.h"
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
           Source source_arg);
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
    // Timestamp for when a tab associated wit the given URL visit was last
    // activated.
    base::Time last_active;
    // Whether there is a tab for the given URL visit that is pinned.
    bool pinned = false;
    // Whether there is a tab for the given URL visit that is part of a group.
    bool in_group = false;
    // The number of opened tabs for the given URL visit aggregate in a time
    // period.
    size_t tab_count = 0;
  };

  struct HistoryData {
    explicit HistoryData(history::AnnotatedVisit annotated_visit);
    ~HistoryData();

    // The last annotated visit associated with the given URL visit in a given
    // time period.
    history::AnnotatedVisit last_visited;

    // Whether any of the annotated visits for the given URL visit aggregate are
    // part of a cluster.
    bool in_cluster = false;

    // The total duration in the foreground for all visits associated with the
    // aggregate in a time period.
    base::TimeDelta total_foreground_duration = base::Seconds(0);

    // The number of history visits associated with the URL visit aggregate in a
    // time period.
    size_t visit_count = 0;
  };

  URLVisitAggregate();
  URLVisitAggregate(const URLVisitAggregate&) = delete;
  URLVisitAggregate(URLVisitAggregate&& other);
  URLVisitAggregate& operator=(URLVisitAggregate&& other);
  ~URLVisitAggregate();

  // Returns a set of associated visit URLs present in the data provided by the
  // various fetchers that participated in constructing the aggregate object.
  std::set<const GURL*> GetAssociatedURLs() const;

  // A map of aggregate tab related characteristics associated with the visit as
  // provided by a given source.
  using URLVisitVariant =
      std::variant<URLVisitAggregate::TabData, URLVisitAggregate::HistoryData>;
  std::map<Fetcher, URLVisitVariant> fetcher_data_map;

  // Whether the visit is bookmarked or not.
  bool bookmarked = false;
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
