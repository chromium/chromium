// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/content_suggestions_metrics.h"

#include <cmath>
#include <string>
#include <type_traits>

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/stringprintf.h"

namespace ntp_snippets {
namespace metrics {

namespace {

// Keep in sync with MAX_SUGGESTIONS_PER_SECTION in NewTabPageUma.java.
const int kMaxSuggestionsPerCategory = 20;

const int kMaxSuggestionsTotal = 50;
const int kMaxCategories = 10;

const char kHistogramCountOnNtpOpenedIfVisible[] =
    "NewTabPage.ContentSuggestions.CountOnNtpOpenedIfVisible";
const char kHistogramSectionCountOnNtpOpened[] =
    "NewTabPage.ContentSuggestions.SectionCountOnNtpOpened";
const char kHistogramShown[] = "NewTabPage.ContentSuggestions.Shown";
const char kHistogramShownAge[] = "NewTabPage.ContentSuggestions.ShownAge";
const char kHistogramShownScore[] =
    "NewTabPage.ContentSuggestions.ShownScoreNormalized";
const char kHistogramOpened[] = "NewTabPage.ContentSuggestions.Opened";
const char kHistogramOpenedAge[] = "NewTabPage.ContentSuggestions.OpenedAge";
const char kHistogramOpenedCategoryIndex[] =
    "NewTabPage.ContentSuggestions.OpenedCategoryIndex";
const char kHistogramOpenedScore[] =
    "NewTabPage.ContentSuggestions.OpenedScoreNormalized";
const char kHistogramOpenDisposition[] =
    "NewTabPage.ContentSuggestions.OpenDisposition";
const char kHistogramMenuOpened[] = "NewTabPage.ContentSuggestions.MenuOpened";
const char kHistogramMenuOpenedAge[] =
    "NewTabPage.ContentSuggestions.MenuOpenedAge";
const char kHistogramMenuOpenedScore[] =
    "NewTabPage.ContentSuggestions.MenuOpenedScoreNormalized";
const char kHistogramDismissedUnvisited[] =
    "NewTabPage.ContentSuggestions.DismissedUnvisited";
const char kHistogramDismissedVisited[] =
    "NewTabPage.ContentSuggestions.DismissedVisited";
const char kHistogramArticlesUsageTimeLocal[] =
    "NewTabPage.ContentSuggestions.UsageTimeLocal";
const char kHistogramVisitDuration[] =
    "NewTabPage.ContentSuggestions.VisitDuration";
const char kHistogramMoreButtonShown[] =
    "NewTabPage.ContentSuggestions.MoreButtonShown";
const char kHistogramMoreButtonClicked[] =
    "NewTabPage.ContentSuggestions.MoreButtonClicked";
const char kHistogramMovedUpCategoryNewIndex[] =
    "NewTabPage.ContentSuggestions.MovedUpCategoryNewIndex";
const char kHistogramCategoryDismissed[] =
    "NewTabPage.ContentSuggestions.CategoryDismissed";
const char kHistogramTimeSinceSuggestionFetched[] =
    "NewTabPage.ContentSuggestions.TimeSinceSuggestionFetched";

// Histograms related to prefetching.
const char kHistogramPrefetchedArticleOpenedWhenOffline[] =
    "NewTabPage.ContentSuggestions.Opened.Articles.Prefetched.Offline";
// NewTabPage.ContentSuggestions.CountOnNtpOpenedIfVisible.Articles.\
// Prefetched.Offline2 and
// NewTabPage.ContentSuggestions.Shown.Articles.Prefetched.Offline2 are recorded
// in Java to avoid race condition.

const char kPerCategoryHistogramFormat[] = "%s.%s";

// This mostly corresponds to the KnownCategories enum, but it is contiguous
// and contains exactly the values to be recorded in UMA. Don't remove or
// reorder elements, only add new ones at the end (before COUNT), and keep in
// sync with ContentSuggestionsCategory in histograms.xml.
enum class HistogramCategories {
  EXPERIMENTAL,
  RECENT_TABS_DEPRECATED,
  DOWNLOADS_DEPRECATED,
  BOOKMARKS_DEPRECATED,
  PHYSICAL_WEB_PAGES_DEPRECATED,
  FOREIGN_TABS_DEPRECATED,
  ARTICLES,
  READING_LIST,
  CONTEXTUAL,
  // Insert new values here!
  COUNT
};

HistogramCategories GetHistogramCategory(Category category) {
  static_assert(
      std::is_same<decltype(category.id()),
                   typename std::underlying_type<KnownCategories>::type>::value,
      "KnownCategories must have the same underlying type as category.id()");
  // Note: Since the underlying type of KnownCategories is int, it's legal to
  // cast from int to KnownCategories, even if the given value isn't listed in
  // the enumeration. The switch still makes sure that all known values are
  // listed here.
  auto known_category = static_cast<KnownCategories>(category.id());
  switch (known_category) {
    case KnownCategories::ARTICLES:
      return HistogramCategories::ARTICLES;
    case KnownCategories::READING_LIST:
      return HistogramCategories::READING_LIST;
    case KnownCategories::BOOKMARKS_DEPRECATED:
      return HistogramCategories::BOOKMARKS_DEPRECATED;
    case KnownCategories::DOWNLOADS_DEPRECATED:
      return HistogramCategories::DOWNLOADS_DEPRECATED;
    case KnownCategories::FOREIGN_TABS_DEPRECATED:
      return HistogramCategories::FOREIGN_TABS_DEPRECATED;
    case KnownCategories::RECENT_TABS_DEPRECATED:
    case KnownCategories::PHYSICAL_WEB_PAGES_DEPRECATED:
    case KnownCategories::LOCAL_CATEGORIES_COUNT:
    case KnownCategories::REMOTE_CATEGORIES_OFFSET:
      NOTREACHED();
      return HistogramCategories::COUNT;
  }
  // All other (unknown) categories go into a single "Experimental" bucket.
  return HistogramCategories::EXPERIMENTAL;
}

// Each suffix here should correspond to an entry under histogram suffix
// ContentSuggestionCategory in histograms.xml.
std::string GetCategorySuffix(Category category) {
  HistogramCategories histogram_category = GetHistogramCategory(category);
  switch (histogram_category) {
    case HistogramCategories::ARTICLES:
      return "Articles";
    case HistogramCategories::EXPERIMENTAL:
      return "Experimental";
    case HistogramCategories::READING_LIST:
      return "ReadingList";
    case HistogramCategories::CONTEXTUAL:
      return "Contextual";
    case HistogramCategories::BOOKMARKS_DEPRECATED:
    case HistogramCategories::DOWNLOADS_DEPRECATED:
    case HistogramCategories::FOREIGN_TABS_DEPRECATED:
    case HistogramCategories::RECENT_TABS_DEPRECATED:
    case HistogramCategories::PHYSICAL_WEB_PAGES_DEPRECATED:
    case HistogramCategories::COUNT:
      NOTREACHED();
      break;
  }
  return std::string();
}

std::string GetCategoryHistogramName(const char* base_name, Category category) {
  return base::StringPrintf(kPerCategoryHistogramFormat, base_name,
                            GetCategorySuffix(category).c_str());
}

// This corresponds to UMA_HISTOGRAM_CUSTOM_TIMES (with min/max appropriate
// for the age of suggestions) for use with dynamic histogram names.
void UmaHistogramAge(const std::string& name, const base::TimeDelta& value) {
  base::Histogram::FactoryTimeGet(
      name, base::TimeDelta::FromSeconds(1), base::TimeDelta::FromDays(7), 100,
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->AddTime(value);
}

void LogCategoryHistogramPosition(const char* base_name,
                                  Category category,
                                  int position,
                                  int max_position) {
  std::string name = GetCategoryHistogramName(base_name, category);
  // Since the histogram name is dynamic, we can't use the regular macro.
  base::UmaHistogramExactLinear(name, position, max_position);
}

void LogCategoryHistogramAge(const char* base_name,
                             Category category,
                             const base::TimeDelta& value) {
  std::string name = GetCategoryHistogramName(base_name, category);
  // Since the histogram name is dynamic, we can't use the regular macro.
  UmaHistogramAge(name, value);
}

void LogCategoryHistogramScore(const char* base_name,
                               Category category,
                               float score) {
  std::string name = GetCategoryHistogramName(base_name, category);
  // Scores are typically reported in a range of (0,1]. As UMA does not support
  // floats, we put them on a discrete scale of [1,10]. We keep the extra bucket
  // 11 for unexpected over-flows as we want to distinguish them from scores
  // close to 1. For instance, the discrete value 1 represents score values
  // within (0.0, 0.1].
  base::UmaHistogramExactLinear(name, ceil(score * 10), 11);
}

// Records ContentSuggestions usage. Therefore the day is sliced into 20min
// buckets. Depending on the current local time the count of the corresponding
// bucket is increased.
void RecordContentSuggestionsUsage() {
  const int kBucketSizeMins = 20;
  const int kNumBuckets = 24 * 60 / kBucketSizeMins;

  base::Time::Exploded now_exploded;
  base::Time::Now().LocalExplode(&now_exploded);
  int bucket = (now_exploded.hour * 60 + now_exploded.minute) / kBucketSizeMins;

  const char* kWeekdayNames[] = {"Sunday",   "Monday", "Tuesday", "Wednesday",
                                 "Thursday", "Friday", "Saturday"};
  std::string histogram_name(
      base::StringPrintf("%s.%s", kHistogramArticlesUsageTimeLocal,
                         kWeekdayNames[now_exploded.day_of_week]));
  base::UmaHistogramExactLinear(histogram_name, bucket, kNumBuckets);
  UMA_HISTOGRAM_EXACT_LINEAR(kHistogramArticlesUsageTimeLocal, bucket,
                             kNumBuckets);

  base::RecordAction(
      base::UserMetricsAction("NewTabPage_ContentSuggestions_ArticlesUsage"));
}

}  // namespace

void OnPageShown(const std::vector<Category>& categories,
                 const std::vector<int>& suggestions_per_category,
                 const std::vector<bool>& is_category_visible) {
  DCHECK_EQ(categories.size(), suggestions_per_category.size());
  DCHECK_EQ(categories.size(), is_category_visible.size());
  int suggestions_total = 0;
  int visible_categories_count = 0;
  for (size_t i = 0; i < categories.size(); ++i) {
    if (is_category_visible[i]) {
      LogCategoryHistogramPosition(kHistogramCountOnNtpOpenedIfVisible,
                                   categories[i], suggestions_per_category[i],
                                   kMaxSuggestionsPerCategory);
      suggestions_total += suggestions_per_category[i];
      ++visible_categories_count;
    }
  }
  UMA_HISTOGRAM_EXACT_LINEAR(kHistogramCountOnNtpOpenedIfVisible,
                             suggestions_total, kMaxSuggestionsTotal);
  UMA_HISTOGRAM_EXACT_LINEAR(kHistogramSectionCountOnNtpOpened,
                             visible_categories_count, kMaxCategories);
}

void OnSuggestionShown(int global_position,
                       Category category,
                       int position_in_category,
                       base::Time publish_date,
                       float score,
                       base::Time fetch_date) {
  UMA_HISTOGRAM_EXACT_LINEAR(kHistogramShown, global_position,
                             kMaxSuggestionsTotal);
  LogCategoryHistogramPosition(kHistogramShown, category, position_in_category,
                               kMaxSuggestionsPerCategory);

  base::TimeDelta age = base::Time::Now() - publish_date;
  LogCategoryHistogramAge(kHistogramShownAge, category, age);

  LogCategoryHistogramScore(kHistogramShownScore, category, score);

  if (category.IsKnownCategory(KnownCategories::ARTICLES)) {
    // Records the time since the fetch time of the displayed snippet.
    UMA_HISTOGRAM_CUSTOM_TIMES(
        kHistogramTimeSinceSuggestionFetched, base::Time::Now() - fetch_date,
        base::TimeDelta::FromSeconds(1), base::TimeDelta::FromDays(7),
        /*bucket_count=*/100);
  }

  // TODO(markusheintz): Discuss whether the code below should be moved into a
  // separate method called OnSuggestionsListShown.
  // When the first of the articles suggestions is shown, then we count this as
  // a single usage of content suggestions.
  if (category.IsKnownCategory(KnownCategories::ARTICLES) &&
      position_in_category == 0) {
    RecordContentSuggestionsUsage();
  }
}

void OnSuggestionOpened(int global_position,
                        Category category,
                        int category_index,
                        int position_in_category,
                        base::Time publish_date,
                        float score,
                        WindowOpenDisposition disposition,
                        bool is_prefetched,
                        bool is_offline) {
  UMA_HISTOGRAM_EXACT_LINEAR(kHistogramOpenedCategoryIndex, category_index,
                             kMaxCategories);
  LogCategoryHistogramPosition(kHistogramOpenedCategoryIndex, category,
                               category_index, kMaxCategories);

  UMA_HISTOGRAM_EXACT_LINEAR(kHistogramOpened, global_position,
                             kMaxSuggestionsTotal);
  LogCategoryHistogramPosition(kHistogramOpened, category, position_in_category,
                               kMaxSuggestionsPerCategory);

  base::TimeDelta age = base::Time::Now() - publish_date;
  LogCategoryHistogramAge(kHistogramOpenedAge, category, age);

  LogCategoryHistogramScore(kHistogramOpenedScore, category, score);

  // We use WindowOpenDisposition::MAX_VALUE + 1 for |value_max| since MAX_VALUE
  // itself is a valid (and used) enum value.
  UMA_HISTOGRAM_EXACT_LINEAR(
      kHistogramOpenDisposition, static_cast<int>(disposition),
      static_cast<int>(WindowOpenDisposition::MAX_VALUE) + 1);
  base::UmaHistogramExactLinear(
      GetCategoryHistogramName(kHistogramOpenDisposition, category),
      static_cast<int>(disposition),
      static_cast<int>(WindowOpenDisposition::MAX_VALUE) + 1);

  if (category.IsKnownCategory(KnownCategories::ARTICLES)) {
    RecordContentSuggestionsUsage();
    if (is_offline && is_prefetched) {
      UMA_HISTOGRAM_EXACT_LINEAR(kHistogramPrefetchedArticleOpenedWhenOffline,
                                 position_in_category,
                                 kMaxSuggestionsPerCategory);
    }
  }

  base::RecordAction(base::UserMetricsAction("Suggestions.Content.Opened"));
}

void OnSuggestionMenuOpened(int global_position,
                            Category category,
                            int position_in_category,
                            base::Time publish_date,
                            float score) {
  UMA_HISTOGRAM_EXACT_LINEAR(kHistogramMenuOpened, global_position,
                             kMaxSuggestionsTotal);
  LogCategoryHistogramPosition(kHistogramMenuOpened, category,
                               position_in_category,
                               kMaxSuggestionsPerCategory);

  base::TimeDelta age = base::Time::Now() - publish_date;
  LogCategoryHistogramAge(kHistogramMenuOpenedAge, category, age);

  LogCategoryHistogramScore(kHistogramMenuOpenedScore, category, score);
}

void OnSuggestionDismissed(int global_position,
                           Category category,
                           int position_in_category,
                           bool visited) {
  if (visited) {
    UMA_HISTOGRAM_EXACT_LINEAR(kHistogramDismissedVisited, global_position,
                               kMaxSuggestionsTotal);
    LogCategoryHistogramPosition(kHistogramDismissedVisited, category,
                                 position_in_category,
                                 kMaxSuggestionsPerCategory);
  } else {
    UMA_HISTOGRAM_EXACT_LINEAR(kHistogramDismissedUnvisited, global_position,
                               kMaxSuggestionsTotal);
    LogCategoryHistogramPosition(kHistogramDismissedUnvisited, category,
                                 position_in_category,
                                 kMaxSuggestionsPerCategory);
  }
}

void OnSuggestionTargetVisited(Category category, base::TimeDelta visit_time) {
  std::string name =
      GetCategoryHistogramName(kHistogramVisitDuration, category);
  base::UmaHistogramLongTimes(name, visit_time);
}

void OnCategoryMovedUp(int new_index) {
  UMA_HISTOGRAM_EXACT_LINEAR(kHistogramMovedUpCategoryNewIndex, new_index,
                             kMaxCategories);
}

void OnMoreButtonShown(Category category, int position) {
  // The "more" card can appear in addition to the actual suggestions, so add
  // one extra bucket to this histogram.
  LogCategoryHistogramPosition(kHistogramMoreButtonShown, category, position,
                               kMaxSuggestionsPerCategory + 1);
}

void OnMoreButtonClicked(Category category, int position) {
  // The "more" card can appear in addition to the actual suggestions, so add
  // one extra bucket to this histogram.
  LogCategoryHistogramPosition(kHistogramMoreButtonClicked, category, position,
                               kMaxSuggestionsPerCategory + 1);
}

void OnCategoryDismissed(Category category) {
  UMA_HISTOGRAM_ENUMERATION(kHistogramCategoryDismissed,
                            GetHistogramCategory(category),
                            HistogramCategories::COUNT);
}

void RecordRemoteSuggestionsProviderState(bool enabled) {
  UMA_HISTOGRAM_BOOLEAN(
      "NewTabPage.ContentSuggestions.Preferences.RemoteSuggestions", enabled);
}

void RecordContentSuggestionDismissed() {
  base::RecordAction(base::UserMetricsAction("Suggestions.Content.Dismissed"));
}

void RecordCategoryDismissed() {
  base::RecordAction(base::UserMetricsAction("Suggestions.Category.Dismissed"));
}

void RecordFetchAction() {
  base::RecordAction(base::UserMetricsAction("Suggestions.Category.Fetch"));
}

}  // namespace metrics
}  // namespace ntp_snippets
