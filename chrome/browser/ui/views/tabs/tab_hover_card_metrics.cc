// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_hover_card_metrics.h"

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr int kMinHoverCardTimeMilliseconds = 0;
constexpr int kMaxHoverCardTimeMilliseconds = 10000;
constexpr int kHoverCardTimeBuckets = 50;
constexpr int kMinHoverCardsSeen = 0;
constexpr int kMaxHoverCardsSeen = 100;
constexpr int kHistogramBucketCount = 50;

// These buckets are modeled after the buckets in components/tab_count_metrics
// but with the very smallest groups collepsed to reduce the number of
// histograms. The values correspond to roughly the 25th, 50th, 75th, 95th, and
// 99th percentile.
constexpr size_t kTabCountBucketMinimums[]{0, 3, 5, 8, 20, 40};
constexpr const char* kTabCountBucketNames[]{".0To2Tabs",   ".3To4Tabs",
                                             ".5To7Tabs",   ".8To19Tabs",
                                             ".20To39Tabs", ".40OrMoreTabs"};
constexpr size_t kNumTabCountBuckets =
    sizeof(kTabCountBucketNames) / sizeof(kTabCountBucketNames[0]);
static_assert(kNumTabCountBuckets == sizeof(kTabCountBucketMinimums) /
                                         sizeof(kTabCountBucketMinimums[0]),
              "Array sizes must be equal.");

// Histograms with STATIC_HISTOGRAM_* need to be declared inline, but this is
// also boilerplate code, so create macros to create standard histogram types to
// use for hover cards.

#define RECORD_COUNT_METRIC(name, count, bucket)                          \
  STATIC_HISTOGRAM_POINTER_GROUP(                                         \
      GetBucketHistogramName((name), (bucket)), static_cast<int>(bucket), \
      static_cast<int>(kNumTabCountBuckets), Add(count),                  \
      base::Histogram::FactoryGet(                                        \
          GetBucketHistogramName((name), (bucket)), kMinHoverCardsSeen,   \
          kMaxHoverCardsSeen, kHistogramBucketCount,                      \
          base::HistogramBase::kUmaTargetedHistogramFlag))

#define RECORD_TIME_METRIC(name, start_time, bucket)                       \
  STATIC_HISTOGRAM_POINTER_GROUP(                                          \
      GetBucketHistogramName((name), (bucket)), static_cast<int>(bucket),  \
      static_cast<int>(kNumTabCountBuckets),                               \
      Add((start_time).is_null()                                           \
              ? 0                                                          \
              : (base::TimeTicks::Now() - (start_time)).InMilliseconds()), \
      base::Histogram::FactoryGet(                                         \
          GetBucketHistogramName((name), (bucket)),                        \
          kMinHoverCardTimeMilliseconds, kMaxHoverCardTimeMilliseconds,    \
          kHoverCardTimeBuckets,                                           \
          base::HistogramBase::kUmaTargetedHistogramFlag))

#if BUILDFLAG(IS_CHROMEOS_ASH)
// UMA histograms that record animation smoothness for fade-in and fade-out
// animations of tab hover card.
void RecordFadeInSmoothness(int smoothness) {
  constexpr char kHoverCardFadeInSmoothnessHistogramName[] =
      "Chrome.Tabs.AnimationSmoothness.HoverCard.FadeIn";
  UMA_HISTOGRAM_PERCENTAGE(kHoverCardFadeInSmoothnessHistogramName, smoothness);
}

void RecordFadeOutSmoothness(int smoothness) {
  constexpr char kHoverCardFadeOutSmoothnessHistogramName[] =
      "Chrome.Tabs.AnimationSmoothness.HoverCard.FadeOut";
  UMA_HISTOGRAM_PERCENTAGE(kHoverCardFadeOutSmoothnessHistogramName,
                           smoothness);
}
#endif

void RecordTimeSinceLastSeenMetric(base::TimeTicks last_seen_time) {
  constexpr base::TimeDelta kMaxHoverCardReshowTimeDelta =
      base::TimeDelta::FromSeconds(5);
  const base::TimeDelta elapsed_time = base::TimeTicks::Now() - last_seen_time;
  if (elapsed_time > kMaxHoverCardReshowTimeDelta)
    return;

  constexpr base::TimeDelta kMinHoverCardReshowTimeDelta =
      base::TimeDelta::FromMilliseconds(1);
  constexpr int kHoverCardHistogramBucketCount = 50;
  UMA_HISTOGRAM_CUSTOM_TIMES(
      TabHoverCardMetrics::kHistogramTimeSinceLastVisible, elapsed_time,
      kMinHoverCardReshowTimeDelta, kMaxHoverCardReshowTimeDelta,
      kHoverCardHistogramBucketCount);
}

}  // namespace

TabHoverCardMetrics::Delegate::~Delegate() = default;

// static
const char TabHoverCardMetrics::kHistogramTimeSinceLastVisible[] =
    "TabHoverCards.TimeSinceLastVisible";

// static
const char
    TabHoverCardMetrics::kHistogramPrefixHoverCardsSeenBeforeSelection[] =
        "TabHoverCards.TabHoverCardsSeenBeforeTabSelection";

// static
const char TabHoverCardMetrics::kHistogramPrefixPreviewsSeenBeforeSelection[] =
    "TabHoverCards.TabPreviewsSeenBeforeTabSelection";

// static
const char TabHoverCardMetrics::kHistogramPrefixTabHoverCardTime[] =
    "TabHoverCards.TabHoverCardViewedTime";

// static
const char TabHoverCardMetrics::kHistogramPrefixTabPreviewTime[] =
    "TabHoverCards.TabHoverCardPreviewTime";

// static
const char TabHoverCardMetrics::kHistogramPrefixLastTabHoverCardTime[] =
    "TabHoverCards.LastTabHoverCardViewedTime";

// static
const char TabHoverCardMetrics::kHistogramPrefixLastTabPreviewTime[] =
    "TabHoverCards.LastTabHoverCardPreviewTime";

TabHoverCardMetrics::TabHoverCardMetrics(Delegate* delegate)
    : delegate_(delegate) {}
TabHoverCardMetrics::~TabHoverCardMetrics() = default;

void TabHoverCardMetrics::TabSelectionChanged() {
  Reset();
}

void TabHoverCardMetrics::InitialCardBeingShown() {
  Reset();
}

void TabHoverCardMetrics::CardFadingIn() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // This may be null during tests, so we can skip if it is.
  auto* const widget = delegate_->GetHoverCardWidget();
  if (widget) {
    throughput_tracker_.emplace(
        widget->GetCompositor()->RequestNewThroughputTracker());
    throughput_tracker_->Start(ash::metrics_util::ForSmoothness(
        base::BindRepeating(&RecordFadeInSmoothness)));
  }
#endif
}

void TabHoverCardMetrics::CardWillBeHidden() {
  if (!last_tab_)
    return;

  RecordTabTimeMetrics();
  times_for_last_tab_ = last_tab_;
  last_visible_timestamp_ = base::TimeTicks::Now();
  last_tab_ = TabHandle();
}

void TabHoverCardMetrics::CardFadingOut() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // This may be null during tests, so we can skip if it is.
  auto* const widget = delegate_->GetHoverCardWidget();
  if (widget) {
    throughput_tracker_.emplace(
        widget->GetCompositor()->RequestNewThroughputTracker());
    throughput_tracker_->Start(ash::metrics_util::ForSmoothness(
        base::BindRepeating(&RecordFadeOutSmoothness)));
  }
#endif
}

void TabHoverCardMetrics::CardFadeComplete() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (throughput_tracker_.has_value())
    throughput_tracker_->Stop();
#endif
}

void TabHoverCardMetrics::CardFadeCanceled() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (throughput_tracker_.has_value())
    throughput_tracker_->Cancel();
#endif
}

void TabHoverCardMetrics::CardFullyVisibleOnTab(TabHandle tab, bool is_active) {
  if (tab == last_tab_)
    return;

  if (last_tab_)
    RecordTabTimeMetrics();
  else
    RecordTimeSinceLastSeenMetric(last_visible_timestamp_);

  last_tab_ = tab;
  last_tab_time_ = base::TimeTicks::Now();
  ++cards_seen_count_;

  // If the tab isn't active and we're done waiting for a preview image, mark
  // the image as seen now.
  if (!is_active && delegate_->HasPreviewImage()) {
    ImageLoadedForTab(tab);
    last_image_time_ = base::TimeTicks::Now();
  } else {
    last_image_time_ = base::TimeTicks();
  }
}

void TabHoverCardMetrics::ImageLoadedForTab(TabHandle tab) {
  if (tab != last_tab_)
    return;

  ++images_seen_count_;
  last_image_time_ = base::TimeTicks::Now();
}

void TabHoverCardMetrics::TabSelectedViaMouse(TabHandle tab) {
  const size_t tab_count = delegate_->GetTabCount();
  const size_t bucket = GetBucketForTabCount(tab_count);
  RECORD_COUNT_METRIC(kHistogramPrefixHoverCardsSeenBeforeSelection,
                      cards_seen_count_, bucket);
  RECORD_COUNT_METRIC(kHistogramPrefixPreviewsSeenBeforeSelection,
                      images_seen_count_, bucket);

  // |last_tab| may have been cleared out if the hide signal arrived before the
  // selection event, so if it's null, use the backup |times_for_last_tab| we
  // stored during the hide event.
  const bool is_last_tab = tab == (last_tab_ ? last_tab_ : times_for_last_tab_);
  RECORD_TIME_METRIC(kHistogramPrefixLastTabHoverCardTime,
                     is_last_tab ? last_tab_time_ : base::TimeTicks(), bucket);
  if (delegate_->ArePreviewsEnabled()) {
    RECORD_TIME_METRIC(kHistogramPrefixLastTabPreviewTime,
                       is_last_tab ? last_image_time_ : base::TimeTicks(),
                       bucket);
  }
}

// static
size_t TabHoverCardMetrics::GetBucketForTabCount(size_t tab_count) {
  for (size_t bucket = 1; bucket < kNumTabCountBuckets; ++bucket) {
    if (tab_count < kTabCountBucketMinimums[bucket])
      return bucket - 1;
  }
  return kNumTabCountBuckets - 1;
}

// static
std::string TabHoverCardMetrics::GetBucketHistogramName(
    const std::string& prefix,
    size_t bucket) {
  DCHECK_LT(bucket, kNumTabCountBuckets);
  return prefix + ".ByTabCount" + kTabCountBucketNames[bucket];
}

void TabHoverCardMetrics::Reset() {
  cards_seen_count_ = 0;
  images_seen_count_ = 0;
  last_tab_ = TabHandle();
  times_for_last_tab_ = TabHandle();
  last_tab_time_ = base::TimeTicks();
  last_image_time_ = base::TimeTicks();
}

void TabHoverCardMetrics::RecordTabTimeMetrics() {
  const size_t tab_count = delegate_->GetTabCount();
  const size_t bucket = GetBucketForTabCount(tab_count);
  if (!last_tab_time_.is_null()) {
    RECORD_TIME_METRIC(kHistogramPrefixTabHoverCardTime, last_tab_time_,
                       bucket);
  }

  if (delegate_->ArePreviewsEnabled()) {
    if (!last_image_time_.is_null()) {
      RECORD_TIME_METRIC(kHistogramPrefixTabPreviewTime, last_image_time_,
                         bucket);
    }
  }
}
