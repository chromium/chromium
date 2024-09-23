// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/history/core/browser/history_types.h"

#include <limits>

#include "base/check.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/history/core/browser/page_usage_data.h"

namespace history {

namespace {

static constexpr float kScoreEpsilon = 1e-8;

}  // namespace

// VisitRow --------------------------------------------------------------------

VisitRow::VisitRow() = default;

VisitRow::VisitRow(URLID arg_url_id,
                   base::Time arg_visit_time,
                   VisitID arg_referring_visit,
                   ui::PageTransition arg_transition,
                   SegmentID arg_segment_id,
                   bool arg_incremented_omnibox_typed_score,
                   VisitID arg_opener_visit)
    : url_id(arg_url_id),
      visit_time(arg_visit_time),
      referring_visit(arg_referring_visit),
      transition(arg_transition),
      segment_id(arg_segment_id),
      incremented_omnibox_typed_score(arg_incremented_omnibox_typed_score),
      opener_visit(arg_opener_visit) {}

VisitRow::~VisitRow() = default;

VisitRow::VisitRow(const VisitRow&) = default;

// VisitedLinkRow --------------------------------------------------------------

bool operator==(const VisitedLinkRow& lhs, const VisitedLinkRow& rhs) {
  return std::tie(lhs.id, lhs.link_url_id, lhs.top_level_url, lhs.frame_url,
                  lhs.visit_count) == std::tie(rhs.id, rhs.link_url_id,
                                               rhs.top_level_url, rhs.frame_url,
                                               rhs.visit_count);
}

bool operator!=(const VisitedLinkRow& lhs, const VisitedLinkRow& rhs) {
  return !(lhs == rhs);
}

bool operator<(const VisitedLinkRow& lhs, const VisitedLinkRow& rhs) {
  return std::tie(lhs.id, lhs.link_url_id, lhs.top_level_url, lhs.frame_url,
                  lhs.visit_count) < std::tie(rhs.id, rhs.link_url_id,
                                              rhs.top_level_url, rhs.frame_url,
                                              rhs.visit_count);
}

// QueryResults ----------------------------------------------------------------

QueryResults::QueryResults() = default;

QueryResults::~QueryResults() = default;

QueryResults::QueryResults(QueryResults&& other) noexcept {
  Swap(&other);
}

QueryResults& QueryResults::operator=(QueryResults&& other) noexcept {
  Swap(&other);
  return *this;
}

const size_t* QueryResults::MatchesForURL(const GURL& url,
                                          size_t* num_matches) const {
  auto found = url_to_results_.find(url);
  if (found == url_to_results_.end()) {
    if (num_matches)
      *num_matches = 0;
    return nullptr;
  }

  // All entries in the map should have at least one index, otherwise it
  // shouldn't be in the map.
  DCHECK(!found->second.empty());
  if (num_matches)
    *num_matches = found->second.size();
  return &found->second.front();
}

void QueryResults::Swap(QueryResults* other) {
  std::swap(reached_beginning_, other->reached_beginning_);
  results_.swap(other->results_);
  url_to_results_.swap(other->url_to_results_);
}

void QueryResults::SetURLResults(std::vector<URLResult>&& results) {
  results_ = std::move(results);

  // Recreate the map for the results_ has been replaced.
  url_to_results_.clear();
  for (size_t i = 0; i < results_.size(); ++i)
    AddURLUsageAtIndex(results_[i].url(), i);
}

void QueryResults::DeleteURL(const GURL& url) {
  // Delete all instances of this URL. We re-query each time since each
  // mutation will cause the indices to change.
  while (const size_t* match_indices = MatchesForURL(url, nullptr))
    DeleteRange(*match_indices, *match_indices);
}

void QueryResults::DeleteRange(size_t begin, size_t end) {
  DCHECK(begin <= end && begin < size() && end < size());

  // First delete the pointers in the given range and store all the URLs that
  // were modified. We will delete references to these later.
  std::set<GURL> urls_modified;
  for (size_t i = begin; i <= end; i++) {
    urls_modified.insert(results_[i].url());
  }

  // Now just delete that range in the vector en masse (the STL ending is
  // exclusive, while ours is inclusive, hence the +1).
  results_.erase(results_.begin() + begin, results_.begin() + end + 1);

  // Delete the indices referencing the deleted entries.
  for (const auto& url : urls_modified) {
    auto found = url_to_results_.find(url);
    if (found == url_to_results_.end()) {
      NOTREACHED_IN_MIGRATION();
      continue;
    }

    // Need a signed loop type since we do -- which may take us to -1.
    for (int match = 0; match < static_cast<int>(found->second.size());
         match++) {
      if (found->second[match] >= begin && found->second[match] <= end) {
        // Remove this reference from the list.
        found->second.erase(found->second.begin() + match);
        match--;
      }
    }

    // Clear out an empty lists if we just made one.
    if (found->second.empty()) {
      url_to_results_.erase(found);
    }
  }

  // Shift all other indices over to account for the removed ones.
  AdjustResultMap(end + 1, std::numeric_limits<size_t>::max(),
                  -static_cast<ptrdiff_t>(end - begin + 1));
}

void QueryResults::AddURLUsageAtIndex(const GURL& url, size_t index) {
  auto found = url_to_results_.find(url);
  if (found != url_to_results_.end()) {
    // The URL is already in the list, so we can just append the new index.
    found->second.push_back(index);
    return;
  }

  // Need to add a new entry for this URL.
  absl::InlinedVector<size_t, 4> new_list;
  new_list.push_back(index);
  url_to_results_[url] = new_list;
}

void QueryResults::AdjustResultMap(size_t begin, size_t end, ptrdiff_t delta) {
  for (auto& url_to_result : url_to_results_) {
    for (size_t match = 0; match < url_to_result.second.size(); match++) {
      size_t match_index = url_to_result.second[match];
      if (match_index >= begin && match_index <= end)
        url_to_result.second[match] += delta;
    }
  }
}

// QueryOptions ----------------------------------------------------------------

QueryOptions::QueryOptions() = default;

QueryOptions::~QueryOptions() = default;

QueryOptions::QueryOptions(const QueryOptions&) = default;

QueryOptions::QueryOptions(QueryOptions&&) noexcept = default;

QueryOptions& QueryOptions::operator=(const QueryOptions&) = default;

QueryOptions& QueryOptions::operator=(QueryOptions&&) noexcept = default;

void QueryOptions::SetRecentDayRange(int days_ago) {
  end_time = base::Time::Now();
  begin_time = end_time - base::Days(days_ago);
}

int64_t QueryOptions::EffectiveBeginTime() const {
  return begin_time.ToInternalValue();
}

int64_t QueryOptions::EffectiveEndTime() const {
  return end_time.is_null() ? std::numeric_limits<int64_t>::max()
                            : end_time.ToInternalValue();
}

int QueryOptions::EffectiveMaxCount() const {
  return max_count ? max_count : std::numeric_limits<int>::max();
}

// QueryURLResult -------------------------------------------------------------

QueryURLResult::QueryURLResult() = default;

QueryURLResult::~QueryURLResult() = default;

QueryURLResult::QueryURLResult(const QueryURLResult&) = default;

QueryURLResult::QueryURLResult(QueryURLResult&&) noexcept = default;

QueryURLResult& QueryURLResult::operator=(const QueryURLResult&) = default;

QueryURLResult& QueryURLResult::operator=(QueryURLResult&&) noexcept = default;

// MostVisitedURL --------------------------------------------------------------

MostVisitedURL::MostVisitedURL() = default;

MostVisitedURL::MostVisitedURL(const GURL& url, const std::u16string& title)
    : url(url), title(title) {}

MostVisitedURL::MostVisitedURL(const MostVisitedURL& other) = default;

MostVisitedURL::MostVisitedURL(MostVisitedURL&& other) noexcept = default;

MostVisitedURL::~MostVisitedURL() = default;

MostVisitedURL& MostVisitedURL::operator=(const MostVisitedURL&) = default;

// FilteredURL -----------------------------------------------------------------

FilteredURL::FilteredURL() = default;

FilteredURL::FilteredURL(const PageUsageData& page_data)
    : url(page_data.GetURL()),
      title(page_data.GetTitle()),
      score(page_data.GetScore()) {
}

FilteredURL::FilteredURL(FilteredURL&& other) noexcept = default;

FilteredURL::~FilteredURL() = default;

// FilteredURL::ExtendedInfo ---------------------------------------------------

FilteredURL::ExtendedInfo::ExtendedInfo() = default;

// GetAllAppIdsResult -------------------------------------------------------

GetAllAppIdsResult::GetAllAppIdsResult() = default;

GetAllAppIdsResult::GetAllAppIdsResult(GetAllAppIdsResult&& other) = default;

GetAllAppIdsResult& GetAllAppIdsResult::operator=(GetAllAppIdsResult&& other) =
    default;

GetAllAppIdsResult::~GetAllAppIdsResult() = default;

// DomainsVisitedResult -------------------------------------------------------

DomainsVisitedResult::DomainsVisitedResult() = default;

DomainsVisitedResult::DomainsVisitedResult(DomainsVisitedResult&& other) =
    default;

DomainsVisitedResult& DomainsVisitedResult::operator=(
    DomainsVisitedResult&& other) = default;

DomainsVisitedResult::~DomainsVisitedResult() = default;

// TopSitesDelta --------------------------------------------------------------

TopSitesDelta::TopSitesDelta() = default;

TopSitesDelta::TopSitesDelta(const TopSitesDelta& other) = default;

TopSitesDelta::~TopSitesDelta() = default;

// Opener
// -----------------------------------------------------------------------

Opener::Opener() : Opener(0, 0, GURL()) {}

Opener::Opener(ContextID context_id, int nav_entry_id, const GURL& url)
    : context_id(context_id), nav_entry_id(nav_entry_id), url(url) {}

Opener::Opener(const Opener& other) = default;

Opener::~Opener() = default;

// HistoryAddPageArgs ---------------------------------------------------------

HistoryAddPageArgs::HistoryAddPageArgs()
    : HistoryAddPageArgs(GURL(),
                         base::Time(),
                         0,
                         0,
                         std::nullopt,
                         GURL(),
                         RedirectList(),
                         ui::PAGE_TRANSITION_LINK,
                         false,
                         SOURCE_BROWSED,
                         false,
                         true,
                         std::nullopt,
                         std::nullopt,
                         std::nullopt,
                         std::nullopt,
                         std::nullopt,
                         std::nullopt,
                         false) {}

HistoryAddPageArgs::HistoryAddPageArgs(
    const GURL& url,
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
    std::optional<std::u16string> title,
    std::optional<GURL> top_level_url,
    std::optional<Opener> opener,
    std::optional<int64_t> bookmark_id,
    std::optional<std::string> app_id,
    std::optional<VisitContextAnnotations::OnVisitFields> context_annotations,
    bool is_ephemeral)
    : url(url),
      time(time),
      context_id(context_id),
      nav_entry_id(nav_entry_id),
      local_navigation_id(local_navigation_id),
      referrer(referrer),
      redirects(redirects),
      transition(transition),
      hidden(hidden),
      visit_source(source),
      did_replace_entry(did_replace_entry),
      consider_for_ntp_most_visited(consider_for_ntp_most_visited),
      title(title),
      top_level_url(top_level_url),
      opener(opener),
      bookmark_id(bookmark_id),
      app_id(app_id),
      context_annotations(std::move(context_annotations)),
      is_ephemeral(is_ephemeral) {}

HistoryAddPageArgs::HistoryAddPageArgs(const HistoryAddPageArgs& other) =
    default;

HistoryAddPageArgs::~HistoryAddPageArgs() = default;

// DomainMetricSet ------------------------------------------------------------

DomainMetricSet::DomainMetricSet() = default;
DomainMetricSet::DomainMetricSet(const DomainMetricSet&) = default;
DomainMetricSet::~DomainMetricSet() = default;
DomainMetricSet& DomainMetricSet::operator=(const DomainMetricSet&) = default;

// ExpireHistoryArgs ----------------------------------------------------------

ExpireHistoryArgs::ExpireHistoryArgs() = default;

ExpireHistoryArgs::ExpireHistoryArgs(const ExpireHistoryArgs& other) = default;

ExpireHistoryArgs::~ExpireHistoryArgs() = default;

void ExpireHistoryArgs::SetTimeRangeForOneDay(base::Time time) {
  begin_time = time.LocalMidnight();

  // Due to DST, leap seconds, etc., the next day at midnight may be more than
  // 24 hours away, so add 36 hours and round back down to midnight.
  end_time = (begin_time + base::Hours(36)).LocalMidnight();
}

// DeletionTimeRange ----------------------------------------------------------

DeletionTimeRange DeletionTimeRange::Invalid() {
  return DeletionTimeRange();
}

DeletionTimeRange DeletionTimeRange::AllTime() {
  return DeletionTimeRange(base::Time(), base::Time::Max());
}

bool DeletionTimeRange::IsValid() const {
  return end_.is_null() || begin_ <= end_;
}

bool DeletionTimeRange::IsAllTime() const {
  return begin_.is_null() && (end_.is_null() || end_.is_max());
}

// DeletionInfo
// ----------------------------------------------------------

// static
DeletionInfo DeletionInfo::ForAllHistory() {
  return DeletionInfo(DeletionTimeRange::AllTime(), false, {}, {},
                      std::nullopt);
}

// static
DeletionInfo DeletionInfo::ForUrls(URLRows deleted_rows,
                                   std::set<GURL> favicon_urls) {
  return DeletionInfo(DeletionTimeRange::Invalid(), false,
                      std::move(deleted_rows), std::move(favicon_urls),
                      std::nullopt);
}

DeletionInfo::DeletionInfo(const DeletionTimeRange& time_range,
                           bool is_from_expiration,
                           URLRows deleted_rows,
                           std::set<GURL> favicon_urls,
                           std::optional<std::set<GURL>> restrict_urls)
    : DeletionInfo(time_range,
                   is_from_expiration,
                   Reason::kOther,
                   std::move(deleted_rows),
                   /*deleted_visit_ids=*/{},
                   std::move(favicon_urls),
                   std::move(restrict_urls)) {}

DeletionInfo::DeletionInfo(const DeletionTimeRange& time_range,
                           bool is_from_expiration,
                           Reason deletion_reason,
                           URLRows deleted_rows,
                           std::set<VisitID> deleted_visit_ids,
                           std::set<GURL> favicon_urls,
                           std::optional<std::set<GURL>> restrict_urls)
    : time_range_(time_range),
      is_from_expiration_(is_from_expiration),
      deletion_reason_(deletion_reason),
      deleted_rows_(std::move(deleted_rows)),
      deleted_visit_ids_(std::move(deleted_visit_ids)),
      favicon_urls_(std::move(favicon_urls)),
      restrict_urls_(std::move(restrict_urls)) {
  // If time_range is all time or invalid, restrict_urls should be empty.
  DCHECK(!time_range_.IsAllTime() || !restrict_urls_.has_value());
  DCHECK(time_range_.IsValid() || !restrict_urls_.has_value());
  // If restrict_urls_ is defined, it should be non-empty.
  DCHECK(!restrict_urls_.has_value() || !restrict_urls_->empty());
}

DeletionInfo::~DeletionInfo() = default;

DeletionInfo::DeletionInfo(DeletionInfo&& other) noexcept = default;

DeletionInfo& DeletionInfo::operator=(DeletionInfo&& rhs) noexcept = default;

// DeletedVisit ----------------------------------------------------------------

DeletedVisit::DeletedVisit(VisitRow visit)
    : visit_row(visit), deleted_visited_link(std::nullopt) {}

DeletedVisit::DeletedVisit(VisitRow visit,
                           DeletedVisitedLink deleted_visited_link)
    : visit_row(visit), deleted_visited_link(deleted_visited_link) {}

DeletedVisit::DeletedVisit(const DeletedVisit& other) = default;
DeletedVisit& DeletedVisit::operator=(const DeletedVisit& other) = default;

DeletedVisit::~DeletedVisit() = default;

// Clusters --------------------------------------------------------------------

VisitContextAnnotations::VisitContextAnnotations() = default;

VisitContextAnnotations::VisitContextAnnotations(
    const VisitContextAnnotations& other) = default;

VisitContextAnnotations::~VisitContextAnnotations() = default;

bool VisitContextAnnotations::operator==(
    const VisitContextAnnotations& other) const {
  return on_visit == other.on_visit &&
         omnibox_url_copied == other.omnibox_url_copied &&
         is_existing_part_of_tab_group == other.is_existing_part_of_tab_group &&
         is_placed_in_tab_group == other.is_placed_in_tab_group &&
         is_existing_bookmark == other.is_existing_bookmark &&
         is_new_bookmark == other.is_new_bookmark &&
         is_ntp_custom_link == other.is_ntp_custom_link &&
         duration_since_last_visit == other.duration_since_last_visit &&
         page_end_reason == other.page_end_reason &&
         total_foreground_duration == other.total_foreground_duration;
}

bool VisitContextAnnotations::operator!=(
    const VisitContextAnnotations& other) const {
  return !(*this == other);
}

bool VisitContextAnnotations::OnVisitFields::operator==(
    const VisitContextAnnotations::OnVisitFields& other) const {
  return browser_type == other.browser_type && window_id == other.window_id &&
         tab_id == other.tab_id && task_id == other.task_id &&
         root_task_id == other.root_task_id &&
         parent_task_id == other.parent_task_id &&
         response_code == other.response_code;
}

bool VisitContextAnnotations::OnVisitFields::operator!=(
    const VisitContextAnnotations::OnVisitFields& other) const {
  return !(*this == other);
}

AnnotatedVisit::AnnotatedVisit() = default;
AnnotatedVisit::AnnotatedVisit(URLRow url_row,
                               VisitRow visit_row,
                               VisitContextAnnotations context_annotations,
                               VisitContentAnnotations content_annotations,
                               VisitID referring_visit_of_redirect_chain_start,
                               VisitID opener_visit_of_redirect_chain_start,
                               VisitSource source)
    : url_row(url_row),
      visit_row(visit_row),
      context_annotations(context_annotations),
      content_annotations(content_annotations),
      referring_visit_of_redirect_chain_start(
          referring_visit_of_redirect_chain_start),
      opener_visit_of_redirect_chain_start(
          opener_visit_of_redirect_chain_start),
      source(source) {}
AnnotatedVisit::AnnotatedVisit(const AnnotatedVisit&) = default;
AnnotatedVisit::AnnotatedVisit(AnnotatedVisit&&) = default;
AnnotatedVisit& AnnotatedVisit::operator=(const AnnotatedVisit&) = default;
AnnotatedVisit& AnnotatedVisit::operator=(AnnotatedVisit&&) = default;
AnnotatedVisit::~AnnotatedVisit() = default;

// static
int ClusterVisit::InteractionStateToInt(ClusterVisit::InteractionState state) {
  return static_cast<int>(state);
}

ClusterVisit::ClusterVisit() = default;
ClusterVisit::~ClusterVisit() = default;
ClusterVisit::ClusterVisit(const ClusterVisit&) = default;
ClusterVisit::ClusterVisit(ClusterVisit&&) = default;
ClusterVisit& ClusterVisit::operator=(const ClusterVisit&) = default;
ClusterVisit& ClusterVisit::operator=(ClusterVisit&&) = default;

ClusterKeywordData::ClusterKeywordData() = default;
ClusterKeywordData::ClusterKeywordData(
    ClusterKeywordData::ClusterKeywordType type,
    float score)
    : type(type), score(score) {}
ClusterKeywordData::ClusterKeywordData(const ClusterKeywordData&) = default;
ClusterKeywordData::ClusterKeywordData(ClusterKeywordData&&) = default;
ClusterKeywordData& ClusterKeywordData::operator=(const ClusterKeywordData&) =
    default;
ClusterKeywordData& ClusterKeywordData::operator=(ClusterKeywordData&&) =
    default;
ClusterKeywordData::~ClusterKeywordData() = default;

bool ClusterKeywordData::operator==(const ClusterKeywordData& data) const {
  return type == data.type && std::fabs(score - data.score) < kScoreEpsilon;
}

std::string ClusterKeywordData::ToString() const {
  return base::StringPrintf("ClusterKeywordData{%d, %f}", type, score);
}

std::ostream& operator<<(std::ostream& out, const ClusterKeywordData& data) {
  out << data.ToString();
  return out;
}

void ClusterKeywordData::MaybeUpdateKeywordType(
    ClusterKeywordData::ClusterKeywordType other_type) {
  if (type < other_type) {
    type = other_type;
  }
}

std::string ClusterKeywordData::GetKeywordTypeLabel() const {
  switch (type) {
    case kUnknown:
      return "Unknown";
    case kEntityCategory:
      return "EntityCategory";
    case kEntityAlias:
      return "EntityAlias";
    case kEntity:
      return "Entity";
    case kSearchTerms:
      return "SearchTerms";
  }
}

Cluster::Cluster() = default;
Cluster::Cluster(int64_t cluster_id,
                 const std::vector<ClusterVisit>& visits,
                 const base::flat_map<std::u16string, ClusterKeywordData>&
                     keyword_to_data_map,
                 bool should_show_on_prominent_ui_surfaces,
                 std::optional<std::u16string> label,
                 std::optional<std::u16string> raw_label,
                 query_parser::Snippet::MatchPositions label_match_positions,
                 std::vector<std::string> related_searches,
                 float search_match_score)
    : cluster_id(cluster_id),
      visits(visits),
      keyword_to_data_map(keyword_to_data_map),
      should_show_on_prominent_ui_surfaces(
          should_show_on_prominent_ui_surfaces),
      label(label),
      raw_label(raw_label),
      label_match_positions(label_match_positions),
      related_searches(related_searches),
      search_match_score(search_match_score) {}
Cluster::Cluster(const Cluster&) = default;
Cluster::Cluster(Cluster&&) = default;
Cluster& Cluster::operator=(const Cluster&) = default;
Cluster& Cluster::operator=(Cluster&&) = default;
Cluster::~Cluster() = default;

const ClusterVisit& Cluster::GetMostRecentVisit() const {
  return *base::ranges::max_element(
      visits, [](auto time1, auto time2) { return time1 < time2; },
      [](const auto& cluster_visit) {
        return cluster_visit.annotated_visit.visit_row.visit_time;
      });
}

std::vector<std::u16string> Cluster::GetKeywords() const {
  std::vector<std::u16string> keywords;
  for (const auto& p : keyword_to_data_map) {
    keywords.push_back(p.first);
  }
  return keywords;
}

}  // namespace history
