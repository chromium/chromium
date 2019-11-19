// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_TYPES_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_TYPES_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/stack_container.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/favicon_base/favicon_types.h"
#include "components/history/core/browser/history_context.h"
#include "components/history/core/browser/url_row.h"
#include "components/history/core/common/thumbnail_score.h"
#include "components/query_parser/query_parser.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace history {

class PageUsageData;

// Container for a list of URLs.
typedef std::vector<GURL> RedirectList;

typedef int64_t FaviconBitmapID;  // Identifier for a bitmap in a favicon.
typedef int64_t SegmentID;        // URL segments for the most visited view.
typedef int64_t IconMappingID;    // For page url and icon mapping.

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

typedef int64_t VisitID;
// Structure to hold the mapping between each visit's id and its source.
typedef std::map<VisitID, VisitSource> VisitSourceMap;

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
           bool arg_incremented_omnibox_typed_score);
  ~VisitRow();

  // ID of this row (visit ID, used a a referrer for other visits).
  VisitID visit_id = 0;

  // Row ID into the URL table of the URL that this page is.
  URLID url_id = 0;

  base::Time visit_time;

  // Indicates another visit that was the referring page for this one.
  // 0 indicates no referrer.
  VisitID referring_visit = 0;

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

  // Compares two visits based on dates, for sorting.
  bool operator<(const VisitRow& other) const {
    return visit_time < other.visit_time;
  }

  // We allow the implicit copy constuctor and operator=.
};

// We pass around vectors of visits a lot
typedef std::vector<VisitRow> VisitVector;

// The basic information associated with a visit (timestamp, type of visit),
// used by HistoryBackend::AddVisits() to create new visits for a URL.
typedef std::pair<base::Time, ui::PageTransition> VisitInfo;

// PageVisit ------------------------------------------------------------------

// Represents a simplified version of a visit for external users. Normally,
// views are only interested in the time, and not the other information
// associated with a VisitRow.
struct PageVisit {
  URLID page_id = 0;
  base::Time visit_time;
};

// QueryResults ----------------------------------------------------------------

// Encapsulates the results of a history query. It supports an ordered list of
// URLResult objects, plus an efficient way of looking up the index of each time
// a given URL appears in those results.
class QueryResults {
 public:
  typedef std::vector<URLResult> URLResultVector;

  QueryResults();
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
  // for entries with the given URL. The array will be |*num_matches| long.
  // |num_matches| can be NULL if the caller is not interested in the number of
  // results (commonly it will only be interested in the first one and can test
  // the pointer for NULL).
  //
  // When there is no match, it will return NULL and |*num_matches| will be 0.
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
  typedef std::map<GURL, base::StackVector<size_t, 4>> URLToResultIndices;

  // Inserts an entry into the |url_to_results_| map saying that the given URL
  // is at the given index in the results_.
  void AddURLUsageAtIndex(const GURL& url, size_t index);

  // Adds |delta| to each index in url_to_results_ in the range [begin,end]
  // (this is inclusive). This is used when inserting or deleting.
  void AdjustResultMap(size_t begin, size_t end, ptrdiff_t delta);

  // Whether the query reaches the beginning of the database.
  bool reached_beginning_ = false;

  // The ordered list of results. The pointers inside this are owned by this
  // QueryResults object.
  URLResultVector results_;

  // Maps URLs to entries in results_.
  URLToResultIndices url_to_results_;

  DISALLOW_COPY_AND_ASSIGN(QueryResults);
};

// QueryOptions ----------------------------------------------------------------

struct QueryOptions {
  QueryOptions();

  // The time range to search for matches in. The beginning is inclusive and
  // the ending is exclusive. Either one (or both) may be null.
  //
  // This will match only the one recent visit of a URL. For text search
  // queries, if the URL was visited in the given time period, but has also
  // been visited more recently than that, it will not be returned. When the
  // text query is empty, this will return the most recent visit within the
  // time range.
  base::Time begin_time;
  base::Time end_time;

  // Sets the query time to the last |days_ago| days to the present time.
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
  query_parser::MatchingAlgorithm matching_algorithm =
      query_parser::MatchingAlgorithm::DEFAULT;

  // Helpers to get the effective parameters values, since a value of 0 means
  // "unspecified".
  int EffectiveMaxCount() const;
  int64_t EffectiveBeginTime() const;
  int64_t EffectiveEndTime() const;
};

// QueryURLResult -------------------------------------------------------------

// QueryURLResult encapsulates the result of a call to HistoryBackend::QueryURL.
struct QueryURLResult {
  QueryURLResult();
  QueryURLResult(const QueryURLResult&);
  QueryURLResult(QueryURLResult&&) noexcept;
  QueryURLResult& operator=(const QueryURLResult&);
  QueryURLResult& operator=(QueryURLResult&&) noexcept;
  ~QueryURLResult();

  // Indicates whether the call to HistoryBackend::QueryURL was successfull
  // or not. If false, then both |row| and |visits| fields are undefined.
  bool success = false;
  URLRow row;
  VisitVector visits;
};

// VisibleVisitCountToHostResult ----------------------------------------------

// VisibleVisitCountToHostResult encapsulates the result of a call to
// HistoryBackend::GetVisibleVisitCountToHost.
struct VisibleVisitCountToHostResult {
  // Indicates whether the call to HistoryBackend::GetVisibleVisitCountToHost
  // was successful or not. If false, then both |count| and |first_visit| are
  // undefined.
  bool success = false;
  int count = 0;
  base::Time first_visit;
};

// MostVisitedURL --------------------------------------------------------------

// Holds the per-URL information of the most visited query.
struct MostVisitedURL {
  MostVisitedURL();
  MostVisitedURL(const GURL& url, const base::string16& title);
  MostVisitedURL(const MostVisitedURL& other);
  MostVisitedURL(MostVisitedURL&& other) noexcept;
  ~MostVisitedURL();

  MostVisitedURL& operator=(const MostVisitedURL&);

  bool operator==(const MostVisitedURL& other) const {
    return url == other.url;
  }

  GURL url;
  base::string16 title;
};

// FilteredURL -----------------------------------------------------------------

// Holds the per-URL information of the filterd url query.
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
  base::string16 title;
  double score = 0.0;
  ExtendedInfo extended_info;
};

// Navigation -----------------------------------------------------------------

// Marshalling structure for AddPage.
struct HistoryAddPageArgs {
  // The default constructor is equivalent to:
  //
  //   HistoryAddPageArgs(
  //       GURL(), base::Time(), NULL, 0, GURL(),
  //       RedirectList(), ui::PAGE_TRANSITION_LINK,
  //       false, SOURCE_BROWSED, false, true,
  //       base::nullopt)
  //
  // TODO(avi): Is ContextID needed, now that we have a globally-unique
  // nav_entry_id? https://crbug.com/859902
  HistoryAddPageArgs();
  HistoryAddPageArgs(const GURL& url,
                     base::Time time,
                     ContextID context_id,
                     int nav_entry_id,
                     const GURL& referrer,
                     const RedirectList& redirects,
                     ui::PageTransition transition,
                     bool hidden,
                     VisitSource source,
                     bool did_replace_entry,
                     bool consider_for_ntp_most_visited,
                     base::Optional<base::string16> title = base::nullopt);
  HistoryAddPageArgs(const HistoryAddPageArgs& other);
  ~HistoryAddPageArgs();

  GURL url;
  base::Time time;
  ContextID context_id;
  int nav_entry_id;
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
  base::Optional<base::string16> title;
};

// TopSites -------------------------------------------------------------------

typedef std::vector<MostVisitedURL> MostVisitedURLList;
typedef std::vector<FilteredURL> FilteredURLList;

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

// Statistics -----------------------------------------------------------------

// HistoryCountResult encapsulates the result of a call to
// HistoryBackend::GetHistoryCount or
// HistoryBackend::CountUniqueHostsVisitedLastMonth.
struct HistoryCountResult {
  // Indicates whether the call was successful or not. If false, then |count|
  // is undefined.
  bool success = false;
  int count = 0;
};

// HistoryLastVisitToHostResult encapsulates the result of a call to
// HistoryBackend::GetLastVisitToHost().
struct HistoryLastVisitToHostResult {
  // Indicates whether the call was successful or not. This can happen if there
  // are internal database errors or the query was called with invalid
  // arguments. |success| will be true and |last_visit| will be null if
  // the host was never visited before. |last_visit| will always be null if
  // |success| is false.
  bool success = false;
  base::Time last_visit;
};

// Favicons -------------------------------------------------------------------

// Used for the mapping between the page and icon.
struct IconMapping {
  IconMapping();
  IconMapping(const IconMapping&);
  IconMapping(IconMapping&&) noexcept;
  ~IconMapping();

  IconMapping& operator=(const IconMapping&);

  // The unique id of the mapping.
  IconMappingID mapping_id = 0;

  // The url of a web page.
  GURL page_url;

  // The unique id of the icon.
  favicon_base::FaviconID icon_id = 0;

  // The url of the icon.
  GURL icon_url;

  // The type of icon.
  favicon_base::IconType icon_type = favicon_base::IconType::kInvalid;
};

// Defines a favicon bitmap and its associated pixel size.
struct FaviconBitmapIDSize {
  FaviconBitmapIDSize();
  ~FaviconBitmapIDSize();

  // The unique id of the favicon bitmap.
  FaviconBitmapID bitmap_id = 0;

  // The pixel dimensions of the associated bitmap.
  gfx::Size pixel_size;
};

enum FaviconBitmapType {
  // The bitmap gets downloaded while visiting its page. Their life-time is
  // bound to the life-time of the corresponding visit in history.
  //  - These bitmaps are re-downloaded when visiting the page again and the
  //  last_updated timestamp is old enough.
  ON_VISIT,

  // The bitmap gets downloaded because it is demanded by some Chrome UI (while
  // not visiting its page). For this reason, their life-time cannot be bound to
  // the life-time of the corresponding visit in history.
  // - These bitmaps are evicted from the database based on the last time they
  //   were requested.
  // - Furthermore, on-demand bitmaps are immediately marked as expired. Hence,
  //   they are always replaced by ON_VISIT favicons whenever their page gets
  //   visited.
  ON_DEMAND
};

// Defines all associated mappings of a given favicon.
struct IconMappingsForExpiry {
  IconMappingsForExpiry();
  IconMappingsForExpiry(const IconMappingsForExpiry& other);
  ~IconMappingsForExpiry();

  // URL of a given favicon.
  GURL icon_url;
  // URLs of all pages mapped to a given favicon
  std::vector<GURL> page_urls;
};

// Defines a favicon bitmap stored in the history backend.
struct FaviconBitmap {
  FaviconBitmap();
  FaviconBitmap(const FaviconBitmap& other);
  ~FaviconBitmap();

  // The unique id of the bitmap.
  FaviconBitmapID bitmap_id = 0;

  // The id of the favicon to which the bitmap belongs to.
  favicon_base::FaviconID icon_id = 0;

  // Time at which |bitmap_data| was last updated.
  base::Time last_updated;

  // Time at which |bitmap_data| was last requested.
  base::Time last_requested;

  // The bits of the bitmap.
  scoped_refptr<base::RefCountedMemory> bitmap_data;

  // The pixel dimensions of bitmap_data.
  gfx::Size pixel_size;
};

struct ExpireHistoryArgs {
  ExpireHistoryArgs();
  ExpireHistoryArgs(const ExpireHistoryArgs& other);
  ~ExpireHistoryArgs();

  // Sets |begin_time| and |end_time| to the beginning and end of the day (in
  // local time) on which |time| occurs.
  void SetTimeRangeForOneDay(base::Time time);

  std::set<GURL> urls;
  base::Time begin_time;
  base::Time end_time;
};

// Represents the time range of a history deletion. If |IsValid()| is false,
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
// If |IsAllHistory()| returns true, all urls haven been deleted.
// In this case, |deleted_rows()| and |favicon_urls()| are undefined.
// Otherwise |deleted_rows()| contains the urls where all visits have been
// removed from history.
// If |expired()| returns true, this deletion is due to a regularly performed
// history expiration. Otherwise it is an explicit deletion due to a user
// action.
class DeletionInfo {
 public:
  // Returns a DeletionInfo that covers all history.
  static DeletionInfo ForAllHistory();
  // Returns a DeletionInfo with invalid time range for the given urls.
  static DeletionInfo ForUrls(URLRows deleted_rows,
                              std::set<GURL> favicon_urls);

  DeletionInfo(const DeletionTimeRange& time_range,
               bool is_from_expiration,
               URLRows deleted_rows,
               std::set<GURL> favicon_urls,
               base::Optional<std::set<GURL>> restrict_urls);

  ~DeletionInfo();
  // Move-only because of potentially large containers.
  DeletionInfo(DeletionInfo&& other) noexcept;
  DeletionInfo& operator=(DeletionInfo&& rhs) noexcept;

  // If IsAllHistory() returns true, all URLs are deleted and |deleted_rows()|
  //  and |favicon_urls()| are undefined.
  bool IsAllHistory() const { return time_range_.IsAllTime(); }

  // If time_range.IsValid() is true, |restrict_urls| (or all URLs if empty)
  // between time_range.begin() and time_range.end() have been removed.
  const DeletionTimeRange& time_range() const { return time_range_; }

  // Restricts deletions within |time_range()|.
  const base::Optional<std::set<GURL>>& restrict_urls() const {
    return restrict_urls_;
  }

  // Returns true, if the URL deletion is due to expiration.
  bool is_from_expiration() const { return is_from_expiration_; }

  // Returns the list of the deleted URLs.
  // Undefined if |IsAllHistory()| returns true.
  const URLRows& deleted_rows() const { return deleted_rows_; }

  // Returns the list of favicon URLs that correspond to the deleted URLs.
  // Undefined if |IsAllHistory()| returns true.
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
  URLRows deleted_rows_;
  std::set<GURL> favicon_urls_;
  base::Optional<std::set<GURL>> restrict_urls_;
  OriginCountAndLastVisitMap deleted_urls_origin_map_;

  DISALLOW_COPY_AND_ASSIGN(DeletionInfo);
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

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_TYPES_H_
