// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_TYPES_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_TYPES_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "components/favicon_base/favicon_types.h"
#include "components/history/core/browser/history_context.h"
#include "components/history/core/browser/keyword_search_term.h"
#include "components/history/core/browser/url_row.h"
#include "components/query_parser/query_parser.h"
#include "components/query_parser/snippet.h"
#include "components/sessions/core/session_id.h"
#include "components/sync_device_info/device_info.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace history {

class PageUsageData;

// Container for a list of URLs.
typedef std::vector<GURL> RedirectList;

typedef int64_t SegmentID;  // URL segments for the most visited view.

// The enumeration of all possible sources of visits is listed below.
// The source will be propagated along with a URL or a visit item
// and eventually be stored in the history database,
// visit_source table specifically.
// Different from page transition types, they describe the origins of visits.
// (Warning): Please don't change any existing values while it is ok to add
// new values when needed.
enum VisitSource {
  SOURCE_SYNCED = 0,     // Synchronized from somewhere else.
  SOURCE_BROWSED = 1,    // User browsed.
  SOURCE_EXTENSION = 2,  // Added by an extension.
  SOURCE_FIREFOX_IMPORTED = 3,
  SOURCE_IE_IMPORTED = 4,
  SOURCE_SAFARI_IMPORTED = 5,
};

// Corresponds to the "id" column of the "visits" SQL table.
typedef int64_t VisitID;
// `kInvalidVisitID` is 0 because SQL AUTOINCREMENT's very first row has
// "id" == 1. Therefore any 0 VisitID is a sentinel null-like value.
constexpr VisitID kInvalidVisitID = 0;
// Corresponds to the "id" column of the "visited_links" SQL table.
using VisitedLinkID = int64_t;
// `kInvalidVisitedLinkID` is 0 because SQL AUTOINCREMENT's very first row has
// "id" == 1. Therefore any 0 VisitedLinkID is a sentinel null-like value.
constexpr VisitedLinkID kInvalidVisitedLinkID = 0;

// Structure to hold the mapping between each visit's id and its source.
typedef std::map<VisitID, VisitSource> VisitSourceMap;

// Constant used to represent that no app_id is used for matching.
inline constexpr std::optional<std::string> kNoAppIdFilter = std::nullopt;

// VisitRow -------------------------------------------------------------------

// Holds all information associated with a specific visit. A visit holds time
// and referrer information for one time a URL is visited.
class VisitRow {
 public:
  VisitRow();
  VisitRow(URLID arg_url_id,
           base::Time arg_visit_time,
           VisitID arg_referring_visit,
           ui::PageTransition arg_transition,
           SegmentID arg_segment_id,
           bool arg_incremented_omnibox_typed_score,
           VisitID arg_opener_visit);
  ~VisitRow();
  VisitRow(const VisitRow&);

  // Compares two visits based on dates, for sorting.
  bool operator<(const VisitRow& other) const {
    return visit_time < other.visit_time;
  }

  // Row ID of this visit in the table. Some nuances with this ID:
  //  - Do NOT assume that a higher `visit_id` implies a more recent visit.
  //    For example: A Mobile phone that recently got back online can sync a
  //    bunch of older visits onto a Desktop machine all at once.
  //  - Do NOT assume that `visit_id` for the same synced visit matches across
  //    devices. This is just a local AUTOINCREMENTed SQL row ID that has no
  //    special meaning or uniqueness guarantee outside of this local machine.
  //  - See `originator_cache_guid` and `originator_visit_id` for more details.
  VisitID visit_id = kInvalidVisitID;

  // Row ID into the URL table of the URL that this page is.
  URLID url_id = 0;

  base::Time visit_time;

  // Indicates another visit that was the redirecting or referring page for this
  // one. 0 (kInvalidVisitId) indicates no referrer/redirect.
  // Note that this corresponds to the "from_visit" column in the visit DB.
  VisitID referring_visit = kInvalidVisitID;

  // In some cases, a visit can have a referrer that is *not* an actual visit in
  // the history DB. In those cases (and only those), this field contains the
  // referrer URL.
  // This can happen e.g. if a URL is opened from the side panel into the main
  // frame, or for visits coming from outside of Chrome, e.g. from the Android
  // Google app.
  GURL external_referrer_url;

  // A combination of bits from PageTransition.
  ui::PageTransition transition = ui::PAGE_TRANSITION_LINK;

  // The segment id (see visitsegment_database.*).
  // If 0, the segment id is null in the table.
  SegmentID segment_id = 0;

  // Record how much time a user has this visit starting from the user
  // opened this visit to the user closed or ended this visit.
  // This includes both active and inactive time as long as
  // the visit was present.
  base::TimeDelta visit_duration;

  // Records whether the visit incremented the omnibox typed score.
  bool incremented_omnibox_typed_score = false;

  // Indicates the visit that opened this one.
  //
  // 0 (kInvalidVisitId) indicates no opener visit. Only non-zero if this visit
  // was directly initiated by open in a new tab, window, or for same-document
  // navigations. It is possible for this to be non-zero and the visit to not
  // exist (i.e., if the visit expired).
  //
  // This differs from `referring_visit` since this links visits across tabs
  // whereas `referring_visit` is only populated if the Referrer is from the
  // same tab.
  VisitID opener_visit = kInvalidVisitID;

  // Specifies whether a navigation should contribute to the Most Visited tiles
  // in the New Tab Page. Note that setting this to true (most common case)
  // doesn't guarantee it's relevant for Most Visited, since other requirements
  // exist (e.g. certain page transition types).
  bool consider_for_ntp_most_visited = true;

  // These are set only for synced visits originating from a different machine.
  // `originator_cache_guid` is the originator machine's unique client ID. It's
  // called a "cache" just to match Chrome Sync's terminology.
  std::string originator_cache_guid;
  // The visit ID of this visit on the originating device, which is *not*
  // comparable to local visit IDs (as in `visit_id` / `referring_visit` /
  // `opener_visit`).
  // Note that even for synced visits, this may be 0, if the visit came from a
  // "legacy" client (which was using Sessions sync rather than History sync).
  VisitID originator_visit_id = kInvalidVisitID;
  // `originator_referring_visit` and `originator_opener_visit` are similar to
  // the non-"originator" versions, but their contents refer to originator visit
  // IDs rather than to local ones.
  // Note that `originator_referring_visit` corresponds to the
  // "originator_from_visit" column in the visit DB.
  VisitID originator_referring_visit = kInvalidVisitID;
  VisitID originator_opener_visit = kInvalidVisitID;
  // Set to true for visits known to Chrome Sync, which can be:
  //  1. Remote visits that have been synced to the local machine.
  //  2. Local visits that have been sent to Sync.
  bool is_known_to_sync = false;
  // If this visit has a transition type of `LINK` or `MANUAL_SUBFRAME`, it will
  // have a corresponding entry in the VisitedLinkDatabase. That unique row ID
  // is stored here. If there is no corresponding entry, the
  // `kInvalidVisitedLinkID` is stored by default. The VisitDatabase has a
  // many-to-one relationship with the VisitedLinkDatabase. As such, more than
  // one visit may correspond to the same VisitedLinkID.
  VisitedLinkID visited_link_id = kInvalidVisitedLinkID;
  // The package name of the app if this visit takes place in Custom Tab opened
  // by an app. This is set only on Android if the Custom Tab knows which app
  // launched it; otherwise remains null.
  std::optional<std::string> app_id = std::nullopt;
  // We allow the implicit copy constructor and operator=.
};

// We pass around vectors of visits a lot
typedef std::vector<VisitRow> VisitVector;

// The basic information associated with a visit (timestamp, type of visit),
// used by HistoryBackend::AddVisits() to create new visits for a URL.
typedef std::pair<base::Time, ui::PageTransition> VisitInfo;

// Specifies the possible reasons a visit (or its annotations) can get updated.
// Used by HistoryBackendNotifier::NotifyVisitUpdated() and
// HistoryBackendObserver::OnVisitUpdated().
// Only used internally and in memory (not persisted), so can be freely changed.
enum class VisitUpdateReason {
  kSetPageLanguage,
  kSetPasswordState,
  kUpdateVisitDuration,
  kUpdateTransition,
  kUpdateSyncedVisit,
  kAddContextAnnotations,
  kSetOnCloseContextAnnotations
};

// VisitedLinkRow --------------------------------------------------------------
// Holds all information globally associated with one visited link (one row in
// the VisitedLinkDatabase).
//
// The VisitedLinkDatabase contains the following fields:
struct VisitedLinkRow {
  // `id` - the unique int64 ID assigned to this row.
  // This is immutable except when retrieving the row from the database or when
  // determining if the visited link referenced by the VisitedLinkRow already
  // exists in the database.
  VisitedLinkID id = 0;

  // `link_url_id` - the unique URLID assigned to the row where this link url is
  // stored in the URLDatabase. ID is stored to avoid storing the URL twice.
  // Immutable except when retrieving the row from the database. If clients want
  // to change it, they must use the constructor to make a new one.
  URLID link_url_id = 0;

  // `top_level_url` - the GURL of the top-level frame where the link url was
  // visited from.
  // Immutable except when retrieving the row from the database. If clients want
  // to change it, they must use the constructor to make a new one.
  GURL top_level_url;

  // `frame_url` - the GURL of the frame where the link was visited from.
  // Immutable except when retrieving the row from the database. If clients want
  // to change it, they must use the constructor to make a new one.
  GURL frame_url;

  // `visit_count` - the number of entries in the VisitDatabase corresponding to
  // this row (must exactly match the <link_url, top_level_url, frame_url>
  // partition key).
  int visit_count = 0;

 private:
  friend bool operator==(const VisitedLinkRow& lhs, const VisitedLinkRow& rhs);
  friend bool operator!=(const VisitedLinkRow& lhs, const VisitedLinkRow& rhs);
  friend bool operator<(const VisitedLinkRow& lhs, const VisitedLinkRow& rhs);
};
using VisitedLinkRows = std::vector<VisitedLinkRow>;

// QueryResults ----------------------------------------------------------------

// Encapsulates the results of a history query. It supports an ordered list of
// URLResult objects, plus an efficient way of looking up the index of each time
// a given URL appears in those results.
class QueryResults {
 public:
  typedef std::vector<URLResult> URLResultVector;

  QueryResults();

  QueryResults(const QueryResults&) = delete;
  QueryResults& operator=(const QueryResults&) = delete;

  ~QueryResults();

  QueryResults(QueryResults&& other) noexcept;
  QueryResults& operator=(QueryResults&& other) noexcept;

  void set_reached_beginning(bool reached) { reached_beginning_ = reached; }
  bool reached_beginning() { return reached_beginning_; }

  size_t size() const { return results_.size(); }
  bool empty() const { return results_.empty(); }

  URLResult& back() { return results_.back(); }
  const URLResult& back() const { return results_.back(); }

  URLResult& operator[](size_t i) { return results_[i]; }
  const URLResult& operator[](size_t i) const { return results_[i]; }

  URLResultVector::const_iterator begin() const { return results_.begin(); }
  URLResultVector::const_iterator end() const { return results_.end(); }
  URLResultVector::const_reverse_iterator rbegin() const {
    return results_.rbegin();
  }
  URLResultVector::const_reverse_iterator rend() const {
    return results_.rend();
  }

  // Returns a pointer to the beginning of an array of all matching indices
  // for entries with the given URL. The array will be `*num_matches` long.
  // `num_matches` can be NULL if the caller is not interested in the number of
  // results (commonly it will only be interested in the first one and can test
  // the pointer for NULL).
  //
  // When there is no match, it will return NULL and `*num_matches` will be 0.
  const size_t* MatchesForURL(const GURL& url, size_t* num_matches) const;

  // Swaps the current result with another. This allows ownership to be
  // efficiently transferred without copying.
  void Swap(QueryResults* other);

  // Set the result vector, the parameter vector will be moved to results_.
  // It means the parameter vector will be empty after calling this method.
  void SetURLResults(std::vector<URLResult>&& results);

  // Removes all instances of the given URL from the result set.
  void DeleteURL(const GURL& url);

  // Deletes the given range of items in the result set.
  void DeleteRange(size_t begin, size_t end);

 private:
  // Maps the given URL to a list of indices into results_ which identify each
  // time an entry with that URL appears. Normally, each URL will have one or
  // very few indices after it, so we optimize this to use statically allocated
  // memory when possible.
  typedef std::map<GURL, absl::InlinedVector<size_t, 4>> URLToResultIndices;

  // Inserts an entry into the `url_to_results_` map saying that the given URL
  // is at the given index in the results_.
  void AddURLUsageAtIndex(const GURL& url, size_t index);

  // Adds `delta` to each index in url_to_results_ in the range [begin,end]
  // (this is inclusive). This is used when inserting or deleting.
  void AdjustResultMap(size_t begin, size_t end, ptrdiff_t delta);

  // Whether the query reaches the beginning of the database.
  bool reached_beginning_ = false;

  // The ordered list of results. The pointers inside this are owned by this
  // QueryResults object.
  URLResultVector results_;

  // Maps URLs to entries in results_.
  URLToResultIndices url_to_results_;
};

// QueryOptions ----------------------------------------------------------------

struct QueryOptions {
  QueryOptions();
  QueryOptions(const QueryOptions&);
  QueryOptions(QueryOptions&&) noexcept;
  QueryOptions& operator=(const QueryOptions&);
  QueryOptions& operator=(QueryOptions&&) noexcept;
  ~QueryOptions();

  // The time range to search for matches in. When `visit_order` is
  // `RECENT_FIRST`, the beginning is inclusive and the ending is exclusive.
  // When `VisitOrder` is `OLDEST_FIRST`, vice versa. Either one (or both) may
  // be null.
  //
  // This will match only the one recent visit of a URL. For text search
  // queries, if the URL was visited in the given time period, but has also
  // been visited more recently than that, it will not be returned. When the
  // text query is empty, this will return the most recent visit within the
  // time range.
  base::Time begin_time;
  base::Time end_time;

  // Sets the query time to the last `days_ago` days to the present time.
  void SetRecentDayRange(int days_ago);

  // The maximum number of results to return. The results will be sorted with
  // the most recent first, so older results may not be returned if there is not
  // enough room. When 0, this will return everything.
  int max_count = 0;

  enum DuplicateHandling {
    // Omit visits for which there is a more recent visit to the same URL.
    // Each URL in the results will appear only once.
    REMOVE_ALL_DUPLICATES,

    // Omit visits for which there is a more recent visit to the same URL on
    // the same day. Each URL will appear no more than once per day, where the
    // day is defined by the local timezone.
    REMOVE_DUPLICATES_PER_DAY,

    // Return all visits without deduping.
    KEEP_ALL_DUPLICATES
  };

  // Allows the caller to specify how duplicate URLs in the result set should
  // be handled.
  DuplicateHandling duplicate_policy = REMOVE_ALL_DUPLICATES;

  // Allows the caller to specify the matching algorithm for text queries.
  // query_parser::MatchingAlgorithm matching_algorithm =
  // query_parser::MatchingAlgorithm::DEFAULT;
  std::optional<query_parser::MatchingAlgorithm> matching_algorithm =
      std::nullopt;

  // Whether the history query should only search through hostnames.
  // When this is true, the matching_algorithm field is ignored.
  bool host_only = false;

  enum VisitOrder {
    RECENT_FIRST,
    OLDEST_FIRST,
  };

  // Whether to prioritize most recent or oldest visits when `max_count` is
  // reached. Will affect visit order as well.
  VisitOrder visit_order = RECENT_FIRST;

  // If nullopt, search doesn't take app_id into consideration.
  std::optional<std::string> app_id = std::nullopt;

  // Helpers to get the effective parameters values, since a value of 0 means
  // "unspecified".
  int EffectiveMaxCount() const;
  int64_t EffectiveBeginTime() const;
  int64_t EffectiveEndTime() const;
};

// QueryURLResult -------------------------------------------------------------

// QueryURLResult encapsulates the result of a call to
// `HistoryBackend::QueryURL()` or
// `HistoryBackend::GetMostRecentVisitsForGurl()`.
struct QueryURLResult {
  QueryURLResult();
  QueryURLResult(const QueryURLResult&);
  QueryURLResult(QueryURLResult&&) noexcept;
  QueryURLResult& operator=(const QueryURLResult&);
  QueryURLResult& operator=(QueryURLResult&&) noexcept;
  ~QueryURLResult();

  // Indicates whether the call was successful. If false, then both `row` and
  // `visits` fields are undefined.
  bool success = false;
  URLRow row;
  VisitVector visits;
};

// VisibleVisitCountToHostResult ----------------------------------------------

// VisibleVisitCountToHostResult encapsulates the result of a call to
// HistoryBackend::GetVisibleVisitCountToHost.
struct VisibleVisitCountToHostResult {
  // Indicates whether the call to HistoryBackend::GetVisibleVisitCountToHost
  // was successful or not. If false, then both `count` and `first_visit` are
  // undefined.
  bool success = false;
  int count = 0;
  base::Time first_visit;
};

// MostVisitedURL --------------------------------------------------------------

// Holds the information for a Most Visited page.
struct MostVisitedURL {
  MostVisitedURL();
  MostVisitedURL(const GURL& url, const std::u16string& title);
  MostVisitedURL(const MostVisitedURL& other);
  MostVisitedURL(MostVisitedURL&& other) noexcept;
  ~MostVisitedURL();

  MostVisitedURL& operator=(const MostVisitedURL&);

  bool operator==(const MostVisitedURL& other) const {
    return url == other.url;
  }

  GURL url;                    // The URL of the page.
  std::u16string title;        // The title of the page.
  int visit_count{0};          // The page visit count.
  base::Time last_visit_time;  // The time of the last visit to the page.
  double score{0.0};           // The frecency score of the page.
};

// FilteredURL -----------------------------------------------------------------

// Holds the per-URL information of the filtered url query.
struct FilteredURL {
  struct ExtendedInfo {
    ExtendedInfo();
    // The absolute number of visits.
    unsigned int total_visits = 0;
    // The number of visits, as seen by the Most Visited NTP pane.
    unsigned int visits = 0;
    // The total number of seconds that the page was open.
    int64_t duration_opened = 0;
    // The time when the page was last visited.
    base::Time last_visit_time;
  };

  FilteredURL();
  explicit FilteredURL(const PageUsageData& data);
  FilteredURL(FilteredURL&& other) noexcept;
  ~FilteredURL();

  GURL url;
  std::u16string title;
  double score = 0.0;
  ExtendedInfo extended_info;
};

// GetAllAppIdsResult ----------------------------------------------------

// GetAllAppIdsResult encapsulates a list of all app IDs found in the
// database entries.
struct GetAllAppIdsResult {
  GetAllAppIdsResult();
  GetAllAppIdsResult(GetAllAppIdsResult&& other);
  GetAllAppIdsResult& operator=(GetAllAppIdsResult&& other);
  ~GetAllAppIdsResult();

  std::vector<std::string> app_ids;
};

// DomainsVisitedResult --------------------------------------------------

// DomainsVisitedResult encapsulates two lists of domains visited locally
// and synced.
struct DomainsVisitedResult {
  DomainsVisitedResult();
  DomainsVisitedResult(DomainsVisitedResult&& other);
  DomainsVisitedResult& operator=(DomainsVisitedResult&& other);
  ~DomainsVisitedResult();

  // Domains visited on this device.
  std::vector<std::string> locally_visited_domains;
  // Domains visited on all devices.
  std::vector<std::string> all_visited_domains;
};

// Opener ---------------------------------------------------------------------

// Contains the information required to determine the VisitID of an opening
// visit.
struct Opener {
  // The default constructor is equivalent to:
  //
  // Opener(nullptr, 0, GURL())
  Opener();
  Opener(ContextID context_id, int nav_entry_id, const GURL& url);
  Opener(const Opener& other);
  ~Opener();

  ContextID context_id;
  int nav_entry_id;
  GURL url;
};

// TopSites -------------------------------------------------------------------

using MostVisitedURLList = std::vector<MostVisitedURL>;
using KeywordSearchTermVisitList =
    std::vector<std::unique_ptr<KeywordSearchTermVisit>>;
using FilteredURLList = std::vector<FilteredURL>;

struct MostVisitedURLWithRank {
  MostVisitedURL url;
  int rank;
};

typedef std::vector<MostVisitedURLWithRank> MostVisitedURLWithRankList;

struct TopSitesDelta {
  TopSitesDelta();
  TopSitesDelta(const TopSitesDelta& other);
  ~TopSitesDelta();

  MostVisitedURLList deleted;
  MostVisitedURLWithRankList added;
  MostVisitedURLWithRankList moved;
};

// Map from origins to a count of matching URLs and the last visited time to any
// URL under that origin.
typedef std::map<GURL, std::pair<int, base::Time>> OriginCountAndLastVisitMap;

// Segments -------------------------------------------------------------------

// Contains device information (i.e. OS Type, Form Factor) for all syncing
// devices (including the local device). Devices are identified by their
// Originator Cache GUID. Has the following shape:
//
// originator_cache_guid : { OsType, FormFactor }
using SyncDeviceInfoMap = std::map<
    std::string,
    std::pair<syncer::DeviceInfo::OsType, syncer::DeviceInfo::FormFactor>>;

// Statistics -----------------------------------------------------------------

// HistoryCountResult encapsulates the result of a call to
// HistoryBackend::GetHistoryCount or
// HistoryBackend::CountUniqueHostsVisitedLastMonth.
struct HistoryCountResult {
  // Indicates whether the call was successful or not. If false, then `count`
  // is undefined.
  bool success = false;
  int count = 0;
};

// DomainDiversity  -----------------------------------------------------------
struct DomainMetricCountType {
  DomainMetricCountType(const int metric_count,
                        const base::Time& metric_start_time)
      : count(metric_count), start_time(metric_start_time) {}
  int count;
  base::Time start_time;
};

// DomainMetricSet represents a set of 1-day, 7-day and 28-day domain visit
// counts whose spanning periods all end at the same time.
struct DomainMetricSet {
  DomainMetricSet();
  DomainMetricSet(const DomainMetricSet&);
  ~DomainMetricSet();

  DomainMetricSet& operator=(const DomainMetricSet&);

  std::optional<DomainMetricCountType> one_day_metric;
  std::optional<DomainMetricCountType> seven_day_metric;
  std::optional<DomainMetricCountType> twenty_eight_day_metric;

  // The end time of the spanning periods. All 3 metrics should have the same
  // end time.
  base::Time end_time;
};

// DomainDiversityResults is a collection of DomainMetricSet's computed for
// a continuous range of end dates. Typically, each DomainMetricSet holds a
// metric set whose 1-day, 7-day and 28-day spanning periods all end at one
// unique midnight in that date range.
using DomainDiversityResults = std::vector<DomainMetricSet>;

// The callback to process all domain diversity metrics. The parameter is a pair
// of results, where the first member counts only local visits, and the second
// counts both local and foreign (synced) visits.
using DomainDiversityCallback = base::OnceCallback<void(
    std::pair<DomainDiversityResults, DomainDiversityResults>)>;

// The bitmask to specify the types of metrics to compute in
// HistoryBackend::GetDomainDiversity()
using DomainMetricBitmaskType = uint32_t;
enum DomainMetricType : DomainMetricBitmaskType {
  kNoMetric = 0,
  kEnableLast1DayMetric = 1 << 0,
  kEnableLast7DayMetric = 1 << 1,
  kEnableLast28DayMetric = 1 << 2
};

// HistoryLastVisitResult encapsulates the result HistoryBackend calls to find
// the last visit to a host or URL.
struct HistoryLastVisitResult {
  // Indicates whether the call was successful or not. This can happen if there
  // are internal database errors or the query was called with invalid
  // arguments. `success` will be true and `last_visit` will be null if
  // the host was never visited before. `last_visit` will always be null if
  // `success` is false.
  bool success = false;
  base::Time last_visit;
};

// DailyVisitsResult contains the result of counting visits to a host over a
// time range.
struct DailyVisitsResult {
  // Indicates whether the call was successful or not. Failure can happen if
  // there are internal database errors or the query was called with invalid
  // arguments.
  bool success = false;
  // Number of days in the time range containing visits to the host.
  int days_with_visits = 0;
  // Total number of visits to the host within the time range.
  int total_visits = 0;
};

struct ExpireHistoryArgs {
  ExpireHistoryArgs();
  ExpireHistoryArgs(const ExpireHistoryArgs& other);
  ~ExpireHistoryArgs();

  // Sets `begin_time` and `end_time` to the beginning and end of the day (in
  // local time) on which `time` occurs.
  void SetTimeRangeForOneDay(base::Time time);

  std::set<GURL> urls;
  base::Time begin_time;
  base::Time end_time;
  std::optional<std::string> restrict_app_id;
};

// Represents the time range of a history deletion. If `IsValid()` is false,
// the time range doesn't apply to this deletion e.g. because only a list of
// urls was deleted.
class DeletionTimeRange {
 public:
  static DeletionTimeRange Invalid();

  static DeletionTimeRange AllTime();

  DeletionTimeRange(base::Time begin, base::Time end)
      : begin_(begin), end_(end) {
    DCHECK(IsValid());
  }

  base::Time begin() const {
    DCHECK(IsValid());
    return begin_;
  }

  base::Time end() const {
    DCHECK(IsValid());
    return end_;
  }

  bool IsValid() const;

  // Returns true if this time range covers history from the beginning of time.
  bool IsAllTime() const;

 private:
  // Creates an invalid time range by assigning impossible start and end times.
  DeletionTimeRange() : begin_(base::Time::Max()), end_(base::Time::Min()) {}

  // Begin of a history deletion.
  base::Time begin_;
  // End of a history deletion.
  base::Time end_;
};

// Describes the urls that have been removed due to a history deletion.
// If `IsAllHistory()` returns true, all urls haven been deleted.
// In this case, `deleted_rows()` and `favicon_urls()` are undefined.
// Otherwise `deleted_rows()` contains the urls where all visits have been
// removed from history.
// If `expired()` returns true, this deletion is due to a regularly performed
// history expiration. Otherwise it is an explicit deletion due to a user
// action.
class DeletionInfo {
 public:
  // Captures the reason for the history deletion.
  enum class Reason {
    // All foreign visits are being deleted (i.e. visits that occurred on
    // another device but were synced to this device).
    kDeleteAllForeignVisits,
    kOther,
  };

  // Returns a DeletionInfo that covers all history.
  static DeletionInfo ForAllHistory();
  // Returns a DeletionInfo with invalid time range for the given urls.
  static DeletionInfo ForUrls(URLRows deleted_rows,
                              std::set<GURL> favicon_urls);

  DeletionInfo(const DeletionTimeRange& time_range,
               bool is_from_expiration,
               URLRows deleted_rows,
               std::set<GURL> favicon_urls,
               std::optional<std::set<GURL>> restrict_urls);
  DeletionInfo(const DeletionTimeRange& time_range,
               bool is_from_expiration,
               Reason deletion_reason,
               URLRows deleted_rows,
               std::set<VisitID> deleted_visit_ids,
               std::set<GURL> favicon_urls,
               std::optional<std::set<GURL>> restrict_urls);

  DeletionInfo(const DeletionInfo&) = delete;
  DeletionInfo& operator=(const DeletionInfo&) = delete;

  ~DeletionInfo();
  // Move-only because of potentially large containers.
  DeletionInfo(DeletionInfo&& other) noexcept;
  DeletionInfo& operator=(DeletionInfo&& rhs) noexcept;

  // If IsAllHistory() returns true, all URLs are deleted and `deleted_rows()`
  //  and `favicon_urls()` are undefined.
  bool IsAllHistory() const { return time_range_.IsAllTime(); }

  // If time_range.IsValid() is true, `restrict_urls` (or all URLs if empty)
  // between time_range.begin() and time_range.end() have been removed.
  const DeletionTimeRange& time_range() const { return time_range_; }

  // Restricts deletions within `time_range()`.
  const std::optional<std::set<GURL>>& restrict_urls() const {
    return restrict_urls_;
  }

  // Returns true, if the URL deletion is due to expiration.
  bool is_from_expiration() const { return is_from_expiration_; }

  // The reason for the history deletion.
  Reason deletion_reason() const { return deletion_reason_; }

  // Returns the list of the deleted URLs.
  // Undefined if `IsAllHistory()` returns true.
  const URLRows& deleted_rows() const { return deleted_rows_; }

  // Returns the list of deleted VisitIDs.
  // Undefined if `IsAllHistory()` returns true.
  const std::set<VisitID>& deleted_visit_ids() const {
    return deleted_visit_ids_;
  }

  // Returns the list of favicon URLs that correspond to the deleted URLs.
  // Undefined if `IsAllHistory()` returns true.
  const std::set<GURL>& favicon_urls() const { return favicon_urls_; }

  // Returns a map from origins with deleted urls to a count of remaining URLs
  // and the last visited time.
  const OriginCountAndLastVisitMap& deleted_urls_origin_map() const {
    // The map should only be accessed after it has been populated.
    DCHECK(deleted_rows_.empty() || !deleted_urls_origin_map_.empty());
    return deleted_urls_origin_map_;
  }

  // Populates deleted_urls_origin_map.
  void set_deleted_urls_origin_map(OriginCountAndLastVisitMap origin_map) {
    DCHECK(deleted_urls_origin_map_.empty());
    deleted_urls_origin_map_ = std::move(origin_map);
  }

 private:
  DeletionTimeRange time_range_;
  bool is_from_expiration_;
  Reason deletion_reason_;
  URLRows deleted_rows_;
  std::set<VisitID> deleted_visit_ids_;
  std::set<GURL> favicon_urls_;
  std::optional<std::set<GURL>> restrict_urls_;
  OriginCountAndLastVisitMap deleted_urls_origin_map_;
};

// When a VisitedLink is deleted from the VisitedLinkDatabase, we notify the
// HistoryService with the following information. In `visited_link_row`, we are
// given a URLID. Callers should obtain the GURL associated with that URLID from
// the URLDatabase and pass it along with this payload.
struct DeletedVisitedLink {
  GURL link_url;
  VisitedLinkRow visited_link_row;
};

// When a Visit is deleted from the the VisitDatabase, we notify the
// HistoryService with the following information. `deleted_visited_link` is
// optional, as not all VisitRow deletions result in a deletion from the
// VisitedLinkDatabase.
struct DeletedVisit {
  DeletedVisit(VisitRow visit);
  DeletedVisit(VisitRow visit, DeletedVisitedLink deleted_visited_link);
  DeletedVisit(const DeletedVisit& other);
  DeletedVisit& operator=(const DeletedVisit& other);
  ~DeletedVisit();

  VisitRow visit_row;
  std::optional<DeletedVisitedLink> deleted_visited_link;
};

// Represents a visit to a domain.
class DomainVisit {
 public:
  DomainVisit(const std::string& domain, base::Time visit_time)
      : domain_(domain), visit_time_(visit_time) {}

  const std::string& domain() const { return domain_; }

  const base::Time visit_time() const { return visit_time_; }

 private:
  std::string domain_;
  base::Time visit_time_;
};

// Clusters --------------------------------------------------------------------

// Context annotations about a page visit collected during the page lifetime.
// This struct encapsulates data that's shared between UKM and the on-device
// storage for `HistoryCluster` metadata, recorded to both when the page
// lifetime ends. This is to ensure that History actually has the visit row
// already written.
struct VisitContextAnnotations {
  VisitContextAnnotations();
  VisitContextAnnotations(const VisitContextAnnotations& other);
  ~VisitContextAnnotations();

  bool operator==(const VisitContextAnnotations& other) const;
  bool operator!=(const VisitContextAnnotations& other) const;

  // Values are persisted; do not reorder or reuse, and only add new values at
  // the end.
  enum class BrowserType {
    kUnknown = 0,
    kTabbed = 1,
    kPopup = 2,
    kCustomTab = 3,
    kAuthTab = 4,
  };

  // Fields known immediately on page load, when the visit is created:
  struct OnVisitFields {
    // The type of browser (tabbed, CCT etc) that produced this visit.
    BrowserType browser_type = BrowserType::kUnknown;

    // The IDs of the window and tab in which the visit happened.
    SessionID window_id = SessionID::InvalidValue();
    SessionID tab_id = SessionID::InvalidValue();

    // Task IDs which can be used to group related visits together. See
    // chrome/browser/complex_tasks.
    int64_t task_id = -1;
    int64_t root_task_id = -1;
    int64_t parent_task_id = -1;

    // The HTTP response code of the navigation.
    int response_code = 0;

    bool operator==(const OnVisitFields& other) const;
    bool operator!=(const OnVisitFields& other) const;
  };

  OnVisitFields on_visit;

  // The remaining fields are "on-close": They are computed and written to the
  // DB later, when the visit is "closed" (i.e. the user navigated away or
  // closed the tab).

  // True if the user has cut or copied the omnibox URL to the clipboard for
  // this page load.
  bool omnibox_url_copied = false;

  // True if the page was in a tab group when the navigation was committed.
  bool is_existing_part_of_tab_group = false;

  // True if the page was NOT part of a tab group when the navigation
  // committed, and IS part of a tab group at the end of the page lifetime.
  bool is_placed_in_tab_group = false;

  // True if this page was a bookmark when the navigation was committed.
  bool is_existing_bookmark = false;

  // True if the page was NOT a bookmark when the navigation was committed and
  // was MADE a bookmark during the page's lifetime. In other words:
  // If `is_existing_bookmark` is true, that implies `is_new_bookmark` is false.
  bool is_new_bookmark = false;

  // True if the page has been explicitly added (by the user) to the list of
  // custom links displayed in the NTP. Links added to the NTP by History
  // TopSites don't count for this.  Always false on Android, because Android
  // does not have NTP custom links.
  bool is_ntp_custom_link = false;

  // The duration since the last visit to this URL in seconds, if the user has
  // visited the URL before. Recorded as -1 (second) if the user has not
  // visited the URL before, or if the History service is unavailable or slow to
  // respond. Any duration that exceeds 30 days will be recorded as 30 days, so
  // in practice, if this duration indicates 30 days, it can be anything from 30
  // to the maximum duration that local history is stored.
  base::TimeDelta duration_since_last_visit = base::Seconds(-1);

  // ---------------------------------------------------------------------------
  // The below metrics are all already recorded by UKM for non-memories reasons.
  // We are duplicating them below to persist on-device and send to an offline
  // model.

  // An opaque integer representing page_load_metrics::PageEndReason.
  // Do not use this directly, as it's a raw integer for serialization, and not
  // a typesafe page_load_metrics::PageEndReason.
  int page_end_reason = 0;

  // The total duration that this visit was in the foreground. Recorded as -1 if
  // not recorded.
  base::TimeDelta total_foreground_duration = base::Seconds(-1);
};

// A `VisitRow` along with its corresponding `URLRow`,
// `VisitContextAnnotations`, and `VisitContentAnnotations`.
struct AnnotatedVisit {
  AnnotatedVisit();
  AnnotatedVisit(URLRow url_row,
                 VisitRow visit_row,
                 VisitContextAnnotations context_annotations,
                 VisitContentAnnotations content_annotations,
                 VisitID referring_visit_of_redirect_chain_start,
                 VisitID opener_visit_of_redirect_chain_start,
                 VisitSource visit);
  AnnotatedVisit(const AnnotatedVisit&);
  AnnotatedVisit(AnnotatedVisit&&);
  AnnotatedVisit& operator=(const AnnotatedVisit&);
  AnnotatedVisit& operator=(AnnotatedVisit&&);
  ~AnnotatedVisit();

  URLRow url_row;
  VisitRow visit_row;
  VisitContextAnnotations context_annotations;
  VisitContentAnnotations content_annotations;
  // The `VisitRow::referring_visit` of the 1st visit in the redirect chain that
  // includes this visit. If this visit is not part of a redirect chain or is
  // the 1st visit in a redirect chain, then it will be
  // `visit_row.referring_visit`. Using the collapsed referring visit is
  // important because redirect visits are omitted from AnnotatedVisits, so
  // the uncollapsed referring visit could refer to an omitted visit.
  VisitID referring_visit_of_redirect_chain_start = 0;
  // The `VisitRow::opener_visit` of the 1st visit in the redirect chain that
  // includes this visit. If this visit is not part of a redirect chain or is
  // the 1st visit in a redirect chain, then it will be
  // `visit_row.opener_visit`. Using the collapsed opener visit is
  // important because opener visits are omitted from AnnotatedVisits, so
  // the uncollapsed opener visit could refer to an omitted visit.
  VisitID opener_visit_of_redirect_chain_start = 0;
  VisitSource source;
};

// `ClusterVisit` tracks duplicate visits to propagate deletes. Only the
// duplicate's URL and visit time are needed to delete it, hence doesn't contain
// all the information contained in e.g. `ClusterVisit`.
struct DuplicateClusterVisit {
  VisitID visit_id = 0;

  // Not persisted; derived from visit_id.
  GURL url = {};

  // Not persisted; derived from visit_id.
  base::Time visit_time = {};
};

// An `AnnotatedVisit` associated with some other metadata from clustering.
struct ClusterVisit {
  // Values are persisted; do not reorder or reuse, and only add new values at
  // the end.
  enum class InteractionState {
    // The default state of visits. Visible in both zero-state and searches.
    kDefault = 0,
    // User has marked the visit hidden. Hidden in all surfaces.
    kHidden = 1,
    // User has dismissed the visit with positive intent. Hidden in the
    // zero-state but still searchable.
    kDone = 2,
  };

  // Used for both persistence and debug logging.
  static int InteractionStateToInt(InteractionState state);

  ClusterVisit();
  ~ClusterVisit();
  ClusterVisit(const ClusterVisit&);
  ClusterVisit(ClusterVisit&&);
  ClusterVisit& operator=(const ClusterVisit&);
  ClusterVisit& operator=(ClusterVisit&&);

  AnnotatedVisit annotated_visit;

  // A floating point score in the range [0, 1] describing how important this
  // visit is to the containing cluster.
  float score = 0.0;

  // Can be changed when the user dismisses or hides the visit.
  InteractionState interaction_state = InteractionState::kDefault;

  // Flagged as true if this cluster visit matches the user's search query.
  // This depends on the user's search query, and should not be persisted. It's
  // a UI-state-specific flag that's convenient to buffer here.
  bool matches_search_query = false;

  // A list of visits that have been de-duplicated into this visit. The parent
  // visit is considered the best visit among all the duplicates, and the worse
  // visits are now contained here. Used for deletions; when the parent visit is
  // deleted, the duplicate visits are deleted as well.
  std::vector<DuplicateClusterVisit> duplicate_visits;

  // The site engagement score of the URL associated with this visit. This
  // should not be used by the UI.
  float engagement_score = 0.0;

  // The visit URL stripped down for aggressive deduping. This GURL may not be
  // navigable or even valid. The stripping on `url_for_deduping` must be
  // strictly more aggressive than on `url_for_display`. This ensures that the
  // UI never shows two visits that look completely identical.
  //
  // The stripping is so aggressive that the URL should not be used alone for
  // deduping. See `SimilarVisitDeDeduperClusterFinalizer` for an example usage
  // that combines this with the page title as a deduping key.
  GURL url_for_deduping;

  // The normalized URL for the visit (i.e. an SRP URL normalized based on the
  // user's default search provider).
  GURL normalized_url;

  // The URL used for display. Computed in the cross-platform code to provide
  // a consistent experience between WebUI and Mobile.
  std::u16string url_for_display;

  // Which positions matched the search query in various fields. This depends on
  // the user's search query, and should not be persisted.
  query_parser::Snippet::MatchPositions title_match_positions;
  query_parser::Snippet::MatchPositions url_for_display_match_positions;

  // The URL of the representative image, which may be empty.
  GURL image_url;
};

// Additional data for a cluster keyword.
struct ClusterKeywordData {
  // Corresponds to `HistoryClusterKeywordType` in
  // tools/metrics/histograms/enums.xml.
  //
  // Types are ordered according to preferences.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum ClusterKeywordType {
    kUnknown = 0,
    kEntityCategory = 1,
    kEntityAlias = 2,
    kEntity = 3,
    kSearchTerms = 4,
    kMaxValue = kSearchTerms
  };

  ClusterKeywordData();
  ClusterKeywordData(ClusterKeywordType type, float score);
  ClusterKeywordData(const ClusterKeywordData&);
  ClusterKeywordData(ClusterKeywordData&&);
  ClusterKeywordData& operator=(const ClusterKeywordData&);
  ClusterKeywordData& operator=(ClusterKeywordData&&);
  ~ClusterKeywordData();
  bool operator==(const ClusterKeywordData& data) const;
  std::string ToString() const;

  // Updates cluster keyword type if a new type is preferred over the existing
  // type.
  void MaybeUpdateKeywordType(ClusterKeywordType other_type);

  // Returns a keyword type label.
  // Only used for logging the UMA metric:
  //   Omnibox.SuggestionUsed.ResumeJourney.ClusterKeywordType.*.CTR.
  //
  // crbug.com/1335975: Remove this method when we remove the histograms.
  std::string GetKeywordTypeLabel() const;

  ClusterKeywordType type = ClusterKeywordData::kUnknown;

  // A floating point score describing how important this keyword is to the
  // containing cluster.
  float score = 0;

  friend std::ostream& operator<<(std::ostream& out,
                                  const ClusterKeywordData& data);
};

// A cluster of `ClusterVisit`s with associated metadata (i.e. `keywords` and
// `should_show_on_prominent_ui_surfaces`).
struct Cluster {
  // Values are not persisted and can be freely changed.
  enum class LabelSource {
    kUnknown,
    kSearch,
    kContentDerivedEntity,
    kHostname,
    kUngroupedVisits,
  };

  Cluster();
  Cluster(int64_t cluster_id,
          const std::vector<ClusterVisit>& visits,
          const base::flat_map<std::u16string, ClusterKeywordData>&
              keyword_to_data_map = {},
          bool should_show_on_prominent_ui_surfaces = true,
          std::optional<std::u16string> label = std::nullopt,
          std::optional<std::u16string> raw_label = std::nullopt,
          query_parser::Snippet::MatchPositions label_match_positions = {},
          std::vector<std::string> related_searches = {},
          float search_match_score = 0);
  Cluster(const Cluster&);
  Cluster(Cluster&&);
  Cluster& operator=(const Cluster&);
  Cluster& operator=(Cluster&&);
  ~Cluster();

  const ClusterVisit& GetMostRecentVisit() const;

  std::vector<std::u16string> GetKeywords() const;

  int64_t cluster_id = 0;
  std::vector<ClusterVisit> visits;

  // A map of keywords to additional data.
  base::flat_map<std::u16string, ClusterKeywordData> keyword_to_data_map;

  // Whether the cluster should be shown prominently on UI surfaces.
  bool should_show_on_prominent_ui_surfaces = true;

  // A suitable label for the cluster. Will be nullopt if no suitable label
  // could be determined.
  std::optional<std::u16string> label;

  // The value of label with any leading or trailing quotation indicators
  // removed.
  std::optional<std::u16string> raw_label;

  // Where the label came from. Determines in which ways we can use `raw_label`.
  // This value may also be used by code to determine the type of the cluster.
  LabelSource label_source = LabelSource::kUnknown;

  // The positions within the label that match the search query, if it exists.
  // This depends on the user's search query, and should not be persisted.
  query_parser::Snippet::MatchPositions label_match_positions;

  // The vector of related searches for the whole cluster. This is derived from
  // the related searches of the constituent visits, and computed in
  // cross-platform code so it's consistent across platforms. Should not be
  // persisted.
  std::vector<std::string> related_searches;

  // A floating point score that's positive if the cluster matches the user's
  // search query, and zero otherwise. This depends on the user's search query,
  // and should not be persisted. It's a UI-state-specific score that's
  // convenient to buffer here.
  float search_match_score = 0.0;

  // Set to true if this cluster was loaded from SQL rather than dynamically
  // generated. Used for UI display only and should not be persisted.
  bool from_persistence = false;

  // Set to true if the triggerability of this cluster (e.g. keywords, should
  // show on prominent UI surfaces) has already been calculated.
  bool triggerability_calculated = false;

  // These are set only for synced visits originating from a different machine.
  // `originator_cache_guid` is the originator machine's unique client ID. It's
  // called a "cache" just to match Chrome Sync's terminology.
  // Note that even for synced clusters, this may be empty if from a legacy
  // client that does not support the sending of this field or the local client
  // does not support populating this field.
  std::string originator_cache_guid;
  // The cluster ID of this cluster on the originating device, which is *not*
  // comparable to local cluster IDs (as in `cluster_id`.)
  // Note that even for synced clusters, this may be 0 if from a legacy client
  // that does not support the sending of this field or the local client does
  // not support populating this field.
  int64_t originator_cluster_id = 0;
};

// Navigation -----------------------------------------------------------------

// Marshalling structure for AddPage.
struct HistoryAddPageArgs {
  // The default constructor is equivalent to:
  //
  //   HistoryAddPageArgs(
  //       GURL(), base::Time(), nullptr, 0, std::nullopt, GURL(),
  //       RedirectList(), ui::PAGE_TRANSITION_LINK,
  //       false, SOURCE_BROWSED, false, true,
  //       std::nullopt, std::nullopt, std::nullopt, std::nullopt,
  //       std::nullopt, std::nullopt, false)
  HistoryAddPageArgs();
  HistoryAddPageArgs(const GURL& url,
                     base::Time time,
                     ContextID context_id,
                     int nav_entry_id,
                     std::optional<int64_t> local_navigation_id,
                     const GURL& referrer,
                     const RedirectList& redirects,
                     ui::PageTransition transition,
                     bool hidden,
                     VisitSource source,
                     bool did_replace_entry,
                     bool consider_for_ntp_most_visited,
                     std::optional<std::u16string> title = std::nullopt,
                     std::optional<GURL> top_level_url = std::nullopt,
                     std::optional<Opener> opener = std::nullopt,
                     std::optional<int64_t> bookmark_id = std::nullopt,
                     std::optional<std::string> app_id = std::nullopt,
                     std::optional<VisitContextAnnotations::OnVisitFields>
                         context_annotations = std::nullopt,
                     bool is_ephemeral = false);
  HistoryAddPageArgs(const HistoryAddPageArgs& other);
  ~HistoryAddPageArgs();

  GURL url;
  base::Time time;
  ContextID context_id;
  int nav_entry_id;
  std::optional<int64_t> local_navigation_id;
  GURL referrer;
  RedirectList redirects;
  ui::PageTransition transition;
  bool hidden;
  VisitSource visit_source;
  bool did_replace_entry;
  // Specifies whether a page visit should contribute to the Most Visited tiles
  // in the New Tab Page. Note that setting this to true (most common case)
  // doesn't guarantee it's relevant for Most Visited, since other requirements
  // exist (e.g. certain page transition types).
  bool consider_for_ntp_most_visited;
  std::optional<std::u16string> title;
  // `top_level_url` is a GURL representing the top-level frame that this
  // navigation originated from.
  std::optional<GURL> top_level_url;
  std::optional<Opener> opener;
  std::optional<int64_t> bookmark_id;
  std::optional<std::string> app_id;
  std::optional<VisitContextAnnotations::OnVisitFields> context_annotations;
  bool is_ephemeral;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_TYPES_H_
