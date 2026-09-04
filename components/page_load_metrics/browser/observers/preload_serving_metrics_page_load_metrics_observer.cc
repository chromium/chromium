// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/preload_serving_metrics_page_load_metrics_observer.h"

#include <array>
#include <string_view>

#include "base/check_is_test.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "components/google/core/common/google_util.h"
#include "components/page_load_metrics/browser/navigation_handle_user_data.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/content_features.h"
#include "ui/base/page_transition_types.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(UsedInstantLoadUma)
enum class UsedInstantLoadUma {
  kNoInstantLoad = 0,
  kPrefetch = 1,
  kPrerender = 2,
  kBFCache = 3,
  kMaxValue = kBFCache,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:UsedInstantLoadUma)

UsedInstantLoadUma ToUsedInstantLoadUma(
    content::UsedInstantLoad used_instant_load) {
  switch (used_instant_load) {
    case content::UsedInstantLoad::kNoInstantLoad:
      return UsedInstantLoadUma::kNoInstantLoad;
    case content::UsedInstantLoad::kPrefetchWithoutPrePrefetch:
    case content::UsedInstantLoad::kPrefetchWithPrePrefetch:
      return UsedInstantLoadUma::kPrefetch;
    case content::UsedInstantLoad::kPrerender:
      return UsedInstantLoadUma::kPrerender;
    case content::UsedInstantLoad::kBFCache:
      return UsedInstantLoadUma::kBFCache;
  }
  NOTREACHED();
}

const char* ToString(UsedInstantLoadUma used_instant_load_uma) {
  switch (used_instant_load_uma) {
    case UsedInstantLoadUma::kNoInstantLoad:
      return "NoInstantLoad";
    case UsedInstantLoadUma::kPrefetch:
      return "Prefetch";
    case UsedInstantLoadUma::kPrerender:
      return "Prerender";
    case UsedInstantLoadUma::kBFCache:
      return "BFCache";
  }
  NOTREACHED();
}

// Returns the histogram suffix for the obsolete
// `PreloadServingMetrics.PageLoad.Clients.PaintTiming.NavigationToFirstContentfulPaint.*`
// histograms before the introduction of InitiatorLocation and SRP variants.
//
// TODO(crbug.com/517725655): Remove this function and obsolete histograms once
// the new histograms are fully rolled out.
const char* GetObsoleteSuffix(content::UsedInstantLoad used_instant_load) {
  switch (used_instant_load) {
    case content::UsedInstantLoad::kPrerender:
      return ".WithPrerender";
    case content::UsedInstantLoad::kPrefetchWithoutPrePrefetch:
    case content::UsedInstantLoad::kPrefetchWithPrePrefetch:
      return ".WithPrefetch";
    case content::UsedInstantLoad::kNoInstantLoad:
      return ".WithoutPreload";
    case content::UsedInstantLoad::kBFCache:
      NOTREACHED();
  }
  NOTREACHED();
}

bool IsPrefetch(content::UsedInstantLoad used_instant_load) {
  switch (used_instant_load) {
    case content::UsedInstantLoad::kPrefetchWithoutPrePrefetch:
    case content::UsedInstantLoad::kPrefetchWithPrePrefetch:
      return true;
    case content::UsedInstantLoad::kNoInstantLoad:
    case content::UsedInstantLoad::kPrerender:
    case content::UsedInstantLoad::kBFCache:
      return false;
  }
  NOTREACHED();
}

std::string GetNavigationInitiatorString(
    content::NavigationHandle& navigation_handle) {
  if (ui::PageTransitionCoreTypeIs(navigation_handle.GetPageTransition(),
                                   ui::PAGE_TRANSITION_RELOAD)) {
    return "Reload";
  }

  if ((navigation_handle.GetPageTransition() &
       ui::PAGE_TRANSITION_FORWARD_BACK) ||
      navigation_handle.IsServedFromBackForwardCache()) {
    int history_offset = navigation_handle.GetNavigationEntryOffset();
    if (history_offset > 0) {
      return "Forward";
    }

    if (history_offset < 0) {
      return "Backward";
    }

    return "Other";
  }

  auto* user_data =
      page_load_metrics::NavigationHandleUserData::GetForNavigationHandle(
          navigation_handle);
  if (user_data) {
    return user_data->navigation_type_string();
  }

  if (navigation_handle.IsRendererInitiated() &&
      navigation_handle.HasUserGesture() &&
      ui::PageTransitionCoreTypeIs(navigation_handle.GetPageTransition(),
                                   ui::PAGE_TRANSITION_LINK)) {
    return "LinkClick";
  }

  return "Other";
}

bool GetServedByLegacySearchPrefetch(
    content::NavigationHandle& navigation_handle) {
  auto* user_data =
      page_load_metrics::NavigationHandleUserData::GetForNavigationHandle(
          navigation_handle);
  return user_data && user_data->is_served_by_legacy_search_prefetch();
}

bool IsEventInForeground(
    const std::optional<base::TimeDelta>& event_time,
    const page_load_metrics::PageLoadMetricsObserverDelegate& delegate) {
  const bool is_prerender =
      delegate.GetPrerenderingState() !=
      page_load_metrics::PrerenderingState::kNoPrerendering;
  return is_prerender ? page_load_metrics::
                            WasActivatedInForegroundOptionalEventInForeground(
                                event_time, delegate)
                      : page_load_metrics::
                            WasStartedInForegroundOptionalEventInForeground(
                                event_time, delegate);
}

}  // namespace

namespace page_load_metrics_internal {

void RecordPreloadServingMetricsByNavigationInitiator(
    content::UsedInstantLoad used_instant_load,
    std::string_view navigation_initiator_string,
    bool is_url_srp) {
  UsedInstantLoadUma used_instant_load_uma =
      ToUsedInstantLoadUma(used_instant_load);

  base::UmaHistogramEnumeration(
      base::StrCat(
          {"PreloadServingMetrics.", navigation_initiator_string, ".All"}),
      used_instant_load_uma);
  if (is_url_srp) {
    base::UmaHistogramEnumeration(
        base::StrCat(
            {"PreloadServingMetrics.", navigation_initiator_string, ".SRP"}),
        used_instant_load_uma);
  }
}

void RecordFirstContentfulPaint(
    base::TimeDelta corrected_first_contentful_paint,
    bool is_in_foreground,
    content::UsedInstantLoad used_instant_load,
    std::string_view navigation_initiator_string,
    bool is_url_srp) {
  // BFCache restores are filtered out in `OnFirstContentfulPaintInPage()`, but
  // unit tests can call this function directly.
  if (used_instant_load == content::UsedInstantLoad::kBFCache) {
    CHECK_IS_TEST();
    return;
  }

  const char* obsolete_suffix = GetObsoleteSuffix(used_instant_load);
  const char* used_instant_load_string =
      ToString(ToUsedInstantLoadUma(used_instant_load));

  PAGE_LOAD_HISTOGRAM(
      base::StrCat({"PreloadServingMetrics.PageLoad.Clients.PaintTiming."
                    "NavigationToFirstContentfulPaint",
                    obsolete_suffix}),
      corrected_first_contentful_paint);

  const std::array<std::string_view, 2> navigation_initiators = {
      "All", navigation_initiator_string};
  static constexpr std::string_view kAllOnly[] = {"All"};
  static constexpr std::string_view kAllAndSrp[] = {"All", "SRP"};
  const base::span<const std::string_view> srp_alls =
      is_url_srp ? base::span<const std::string_view>(kAllAndSrp)
                 : base::span<const std::string_view>(kAllOnly);
  const std::array<std::string_view, 2> used_instant_loads = {
      "All", used_instant_load_string};

  if (is_in_foreground) {
    for (const auto navigation_initiator : navigation_initiators) {
      for (const auto srp_all : srp_alls) {
        for (const auto instant_load : used_instant_loads) {
          PAGE_LOAD_HISTOGRAM(
              base::StrCat(
                  {"PreloadServingMetrics.PageLoad.Clients.PaintTiming."
                   "NavigationToFirstContentfulPaint.",
                   navigation_initiator, ".", srp_all, ".", instant_load}),
              corrected_first_contentful_paint);
        }
      }
    }
  }

  for (const auto navigation_initiator : navigation_initiators) {
    for (const auto srp_all : srp_alls) {
      for (const auto instant_load : used_instant_loads) {
        PAGE_LOAD_HISTOGRAM(
            base::StrCat({"PreloadServingMetrics.PageLoad.Clients.PaintTiming."
                          "NavigationToFirstContentfulPaint."
                          "WithoutFiltering.",
                          navigation_initiator, ".", srp_all, ".",
                          instant_load}),
            corrected_first_contentful_paint);
      }
    }
  }

  if (IsPrefetch(used_instant_load)) {
    if (base::FeatureList::IsEnabled(features::kPrefetchOffTheMainThread)) {
      const char* pre_prefetch_suffix =
          used_instant_load ==
                  content::UsedInstantLoad::kPrefetchWithPrePrefetch
              ? ".WithPrePrefetch"
              : ".WithoutPrePrefetch";
      PAGE_LOAD_HISTOGRAM(
          base::StrCat({"PreloadServingMetrics.PageLoad.Clients.PaintTiming."
                        "NavigationToFirstContentfulPaint.WithPrefetch",
                        pre_prefetch_suffix}),
          corrected_first_contentful_paint);

      if (is_in_foreground) {
        for (const auto navigation_initiator : navigation_initiators) {
          for (const auto srp_all : srp_alls) {
            PAGE_LOAD_HISTOGRAM(
                base::StrCat(
                    {"PreloadServingMetrics.PageLoad.Clients.PaintTiming."
                     "NavigationToFirstContentfulPaint.",
                     navigation_initiator, ".", srp_all, ".Prefetch",
                     pre_prefetch_suffix}),
                corrected_first_contentful_paint);
          }
        }
      }

      for (const auto navigation_initiator : navigation_initiators) {
        for (const auto srp_all : srp_alls) {
          PAGE_LOAD_HISTOGRAM(
              base::StrCat(
                  {"PreloadServingMetrics.PageLoad.Clients.PaintTiming."
                   "NavigationToFirstContentfulPaint."
                   "WithoutFiltering.",
                   navigation_initiator, ".", srp_all, ".Prefetch",
                   pre_prefetch_suffix}),
              corrected_first_contentful_paint);
        }
      }
    }
  }
}

void RecordLargestContentfulPaint(
    base::TimeDelta corrected_largest_contentful_paint,
    content::UsedInstantLoad used_instant_load,
    std::string_view navigation_initiator_string,
    bool is_url_srp) {
  // BFCache restores are filtered out in `MaybeRecord()`, but unit tests can
  // call this function directly.
  if (used_instant_load == content::UsedInstantLoad::kBFCache) {
    CHECK_IS_TEST();
    return;
  }

  const char* used_instant_load_string =
      ToString(ToUsedInstantLoadUma(used_instant_load));

  const std::array<std::string_view, 2> navigation_initiators = {
      "All", navigation_initiator_string};
  static constexpr std::string_view kAllOnly[] = {"All"};
  static constexpr std::string_view kAllAndSrp[] = {"All", "SRP"};
  const base::span<const std::string_view> srp_alls =
      is_url_srp ? base::span<const std::string_view>(kAllAndSrp)
                 : base::span<const std::string_view>(kAllOnly);
  const std::array<std::string_view, 2> used_instant_loads = {
      "All", used_instant_load_string};

  for (const auto navigation_initiator : navigation_initiators) {
    for (const auto srp_all : srp_alls) {
      for (const auto instant_load : used_instant_loads) {
        PAGE_LOAD_HISTOGRAM(
            base::StrCat({"PreloadServingMetrics.PageLoad.Clients.PaintTiming."
                          "NavigationToLargestContentfulPaint2.",
                          navigation_initiator, ".", srp_all, ".",
                          instant_load}),
            corrected_largest_contentful_paint);
      }
    }
  }

  if (IsPrefetch(used_instant_load)) {
    if (base::FeatureList::IsEnabled(features::kPrefetchOffTheMainThread)) {
      const char* pre_prefetch_suffix =
          used_instant_load ==
                  content::UsedInstantLoad::kPrefetchWithPrePrefetch
              ? ".WithPrePrefetch"
              : ".WithoutPrePrefetch";
      for (const auto navigation_initiator : navigation_initiators) {
        for (const auto srp_all : srp_alls) {
          PAGE_LOAD_HISTOGRAM(
              base::StrCat(
                  {"PreloadServingMetrics.PageLoad.Clients.PaintTiming."
                   "NavigationToLargestContentfulPaint2.",
                   navigation_initiator, ".", srp_all, ".Prefetch",
                   pre_prefetch_suffix}),
              corrected_largest_contentful_paint);
        }
      }
    }
  }
}

}  // namespace page_load_metrics_internal

PreloadServingMetricsPageLoadMetricsObserver::NavigationData::NavigationData(
    content::NavigationHandle& navigation_handle,
    bool used_bfcache)
    : preload_serving_metrics_capsule(
          content::PreloadServingMetricsCapsule::TakeFromNavigationHandle(
              navigation_handle)),
      used_bfcache(used_bfcache),
      navigation_initiator_string(
          GetNavigationInitiatorString(navigation_handle)),
      is_url_srp(google_util::IsGoogleSearchUrl(navigation_handle.GetURL())),
      is_served_by_legacy_search_prefetch(
          GetServedByLegacySearchPrefetch(navigation_handle)) {
  CHECK(preload_serving_metrics_capsule);
}

PreloadServingMetricsPageLoadMetricsObserver::NavigationData::
    ~NavigationData() = default;
PreloadServingMetricsPageLoadMetricsObserver::NavigationData::NavigationData(
    NavigationData&&) = default;
PreloadServingMetricsPageLoadMetricsObserver::NavigationData&
PreloadServingMetricsPageLoadMetricsObserver::NavigationData::operator=(
    NavigationData&&) = default;

PreloadServingMetricsPageLoadMetricsObserver::
    PreloadServingMetricsPageLoadMetricsObserver() = default;

PreloadServingMetricsPageLoadMetricsObserver::
    ~PreloadServingMetricsPageLoadMetricsObserver() = default;

const char* PreloadServingMetricsPageLoadMetricsObserver::GetObserverName()
    const {
  static const char kName[] = "PreloadServingMetricsPageLoadMetricsObserver";
  return kName;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreloadServingMetricsPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreloadServingMetricsPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreloadServingMetricsPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreloadServingMetricsPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle) {
    return STOP_OBSERVING;
  }

  if (navigation_handle->IsInPrerenderedMainFrame()) {
    // Wait prerender activation.
    return CONTINUE_OBSERVING;
  }

  navigation_data_.emplace(*navigation_handle, /*used_bfcache=*/false);

  return CONTINUE_OBSERVING;
}

void PreloadServingMetricsPageLoadMetricsObserver::DidActivatePrerenderedPage(
    content::NavigationHandle* navigation_handle) {
  navigation_data_.emplace(*navigation_handle, /*used_bfcache=*/false);
}

void PreloadServingMetricsPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // `navigation_data_` can be null if the page entered BackForwardCache before
  // FCP occurred (which resets `navigation_data_`) or if FCP timing is
  // delivered out of order.
  //
  // TODO(https://crbug.com/539388005): `PLMO::OnFirstContentfulPaintInPage()`
  // is not expected to be called between `PLMO::OnEnterBackForwardCache()` and
  // `PLMO::OnRestoreFromBackForwardCache()`, but currently it is happening.
  if (!navigation_data_) {
    return;
  }

  if (navigation_data_->used_bfcache) {
    return;
  }

  bool is_in_foreground = IsEventInForeground(
      timing.paint_timing->first_contentful_paint, GetDelegate());
  base::TimeDelta corrected =
      page_load_metrics::CorrectEventAsNavigationOrActivationOrigined(
          GetDelegate(), timing.paint_timing->first_contentful_paint.value());
  content::UsedInstantLoad used_instant_load =
      navigation_data_->preload_serving_metrics_capsule->GetUsedInstantLoad(
          navigation_data_->used_bfcache,
          navigation_data_->is_served_by_legacy_search_prefetch);

  page_load_metrics_internal::RecordFirstContentfulPaint(
      corrected, is_in_foreground, used_instant_load,
      navigation_data_->navigation_initiator_string,
      navigation_data_->is_url_srp);
}

void PreloadServingMetricsPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  MaybeRecord();
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreloadServingMetricsPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  MaybeRecord();
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreloadServingMetricsPageLoadMetricsObserver::OnEnterBackForwardCache(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  MaybeRecord();
  navigation_data_.reset();
  return CONTINUE_OBSERVING;
}

void PreloadServingMetricsPageLoadMetricsObserver::
    OnRestoreFromBackForwardCache(
        const page_load_metrics::mojom::PageLoadTiming& timing,
        content::NavigationHandle* navigation_handle) {
  navigation_data_.emplace(*navigation_handle, /*used_bfcache=*/true);
}

void PreloadServingMetricsPageLoadMetricsObserver::MaybeRecord() {
  // Record if the navigation is non prerender and committed; or if the
  // navigations are prerender initial/activation navigation and activated.

  if (!navigation_data_) {
    return;
  }

  navigation_data_->preload_serving_metrics_capsule
      ->RecordMetricsForNonPrerenderNavigationCommitted();
  // TODO(https://crbug.com/517725655): PreloadServingMetricsCapsule is taken
  // for BFCache, and this part should be re-visited again to explore better
  // ways to record this case.
  content::UsedInstantLoad used_instant_load =
      navigation_data_->preload_serving_metrics_capsule->GetUsedInstantLoad(
          navigation_data_->used_bfcache,
          navigation_data_->is_served_by_legacy_search_prefetch);

  page_load_metrics_internal::RecordPreloadServingMetricsByNavigationInitiator(
      used_instant_load, navigation_data_->navigation_initiator_string,
      navigation_data_->is_url_srp);

  if (!navigation_data_->used_bfcache) {
    const page_load_metrics::ContentfulPaintTimingInfo&
        all_frames_largest_contentful_paint =
            GetDelegate()
                .GetLargestContentfulPaintHandler()
                .MergeMainFrameAndSubframes();
    if (all_frames_largest_contentful_paint.ContainsValidTime()) {
      const bool is_in_foreground = IsEventInForeground(
          all_frames_largest_contentful_paint.Time(), GetDelegate());
      if (is_in_foreground) {
        base::TimeDelta corrected =
            page_load_metrics::CorrectEventAsNavigationOrActivationOrigined(
                GetDelegate(),
                all_frames_largest_contentful_paint.Time().value());
        page_load_metrics_internal::RecordLargestContentfulPaint(
            corrected, used_instant_load,
            navigation_data_->navigation_initiator_string,
            navigation_data_->is_url_srp);
      }
    }
  }
}
