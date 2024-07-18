// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/abandoned_page_load_metrics_observer.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/browser/navigation_handle.h"

namespace {

using AbandonReason = AbandonedPageLoadMetricsObserver::AbandonReason;

AbandonReason DiscardReasonToAbandonReason(
    content::NavigationDiscardReason discard_reason) {
  switch (discard_reason) {
    case content::NavigationDiscardReason::kNewReloadNavigation:
      return AbandonReason::kNewReloadNavigation;
    case content::NavigationDiscardReason::kNewHistoryNavigation:
      return AbandonReason::kNewHistoryNavigation;
    case content::NavigationDiscardReason::kNewOtherNavigationBrowserInitiated:
      return AbandonReason::kNewOtherNavigationBrowserInitiated;
    case content::NavigationDiscardReason::kNewOtherNavigationRendererInitiated:
      return AbandonReason::kNewOtherNavigationRendererInitiated;
    case content::NavigationDiscardReason::kWillRemoveFrame:
      return AbandonReason::kFrameRemoved;
    case content::NavigationDiscardReason::kExplicitCancellation:
      return AbandonReason::kExplicitCancellation;
    case content::NavigationDiscardReason::kInternalCancellation:
      return AbandonReason::kInternalCancellation;
    case content::NavigationDiscardReason::kRenderProcessGone:
      return AbandonReason::kRenderProcessGone;
    case content::NavigationDiscardReason::kNeverStarted:
      return AbandonReason::kNeverStarted;
    case content::NavigationDiscardReason::kFailedSecurityCheck:
      return AbandonReason::kFailedSecurityCheck;
      // Other cases like kCommittedNavigation and kRenderFrameHostDestruction
      // should be obsolete, so just use "other" as the reason.
    case content::NavigationDiscardReason::kCommittedNavigation:
    case content::NavigationDiscardReason::kRenderFrameHostDestruction:
      return AbandonReason::kOther;
  }
}
}  // namespace

namespace internal {

const char kAbandonedPageLoadMetricsHistogramPrefix[] =
    "PageLoad.Clients.Leakage.";

const char kAbandonReasonNewReloadNavigation[] = "NewReloadNavigation";
const char kAbandonReasonNewHistoryNavigation[] = "NewHistoryNavigation";
const char kAbandonReasonNewOtherNavigationBrowserInitiated[] =
    "NewOtherNavigationBrowserInitiated";
const char kAbandonReasonNewOtherNavigationRendererInitiated[] =
    "NewOtherNavigationRendererInitiated";
const char kAbandonReasonFrameRemoved[] = "FrameRemoved";
const char kAbandonReasonExplicitCancellation[] = "ExplicitCancellation";
const char kAbandonReasonInternalCancellation[] = "InternalCancellation";
const char kAbandonReasonRenderProcessGone[] = "RenderProcessGone";
const char kAbandonReasonNeverStarted[] = "NeverStarted";
const char kAbandonReasonFailedSecurityCheck[] = "FailedSecurityCheck";
const char kAbandonReasonOther[] = "Other";
const char kAbandonReasonHidden[] = "Hidden";
const char kAbandonReasonErrorPage[] = "ErrorPage";
const char kAbandonReasonAppBackgrounded[] = "AppBackgrounded";

const char kSuffixWasBackgrounded[] = ".WasBackgrounded";
const char kSuffixWasHidden[] = ".WasHidden";

const char kMilestoneNavigationStart[] = "NavigationStart";
const char kMilestoneLoaderStart[] = "LoaderStart";
const char kMilestoneFirstRedirectedRequestStart[] =
    "FirstRedirectedRequestStart";
const char kMilestoneFirstRedirectResponseStart[] =
    "FirstRedirectResponseStart";
const char kMilestoneFirstRedirectResponseLoaderCallback[] =
    "FirstRedirectResponseLoaderCallback";
const char kMilestoneNonRedirectedRequestStart[] = "NonRedirectedRequestStart";
const char kMilestoneNonRedirectResponseStart[] = "NonRedirectResponseStart";
const char kMilestoneNonRedirectResponseLoaderCallback[] =
    "NonRedirectResponseLoaderCallback";
const char kMilestoneCommitSent[] = "CommitSent";
const char kMilestoneDidCommit[] = "DidCommit";
const char kMilestoneParseStart[] = "ParseStart";

// TODO(https://crbug.com/347706997): Record more milestones related to loading
// and process creation timing.

}  // namespace internal

std::string AbandonedPageLoadMetricsObserver::AbandonReasonToString(
    AbandonReason abandon_reason) {
  switch (abandon_reason) {
    case AbandonReason::kNewReloadNavigation:
      return internal::kAbandonReasonNewReloadNavigation;
    case AbandonReason::kNewHistoryNavigation:
      return internal::kAbandonReasonNewHistoryNavigation;
    case AbandonReason::kNewOtherNavigationBrowserInitiated:
      return internal::kAbandonReasonNewOtherNavigationBrowserInitiated;
    case AbandonReason::kNewOtherNavigationRendererInitiated:
      return internal::kAbandonReasonNewOtherNavigationRendererInitiated;
    case AbandonReason::kFrameRemoved:
      return internal::kAbandonReasonFrameRemoved;
    case AbandonReason::kExplicitCancellation:
      return internal::kAbandonReasonExplicitCancellation;
    case AbandonReason::kInternalCancellation:
      return internal::kAbandonReasonInternalCancellation;
    case AbandonReason::kRenderProcessGone:
      return internal::kAbandonReasonRenderProcessGone;
    case AbandonReason::kNeverStarted:
      return internal::kAbandonReasonNeverStarted;
    case AbandonReason::kFailedSecurityCheck:
      return internal::kAbandonReasonFailedSecurityCheck;
    case AbandonReason::kOther:
      return internal::kAbandonReasonOther;
    case AbandonReason::kHidden:
      return internal::kAbandonReasonHidden;
    case AbandonReason::kErrorPage:
      return internal::kAbandonReasonErrorPage;
    case AbandonReason::kAppBackgrounded:
      return internal::kAbandonReasonAppBackgrounded;
  }
}

std::string AbandonedPageLoadMetricsObserver::NavigationMilestoneToString(
    NavigationMilestone navigation_milestone) {
  switch (navigation_milestone) {
    case NavigationMilestone::kNavigationStart:
      return internal::kMilestoneNavigationStart;
    case NavigationMilestone::kLoaderStart:
      return internal::kMilestoneLoaderStart;
    case NavigationMilestone::kFirstRedirectedRequestStart:
      return internal::kMilestoneFirstRedirectedRequestStart;
    case NavigationMilestone::kFirstRedirectResponseStart:
      return internal::kMilestoneFirstRedirectResponseStart;
    case NavigationMilestone::kFirstRedirectResponseLoaderCallback:
      return internal::kMilestoneFirstRedirectResponseLoaderCallback;
    case NavigationMilestone::kNonRedirectedRequestStart:
      return internal::kMilestoneNonRedirectedRequestStart;
    case NavigationMilestone::kNonRedirectResponseStart:
      return internal::kMilestoneNonRedirectResponseStart;
    case NavigationMilestone::kNonRedirectResponseLoaderCallback:
      return internal::kMilestoneNonRedirectResponseLoaderCallback;
    case NavigationMilestone::kCommitSent:
      return internal::kMilestoneCommitSent;
    case NavigationMilestone::kDidCommit:
      return internal::kMilestoneDidCommit;
    case NavigationMilestone::kParseStart:
      return internal::kMilestoneParseStart;
  }
}

AbandonedPageLoadMetricsObserver::AbandonedPageLoadMetricsObserver() = default;

AbandonedPageLoadMetricsObserver::~AbandonedPageLoadMetricsObserver() = default;

const char* AbandonedPageLoadMetricsObserver::GetObserverName() const {
  static const char kName[] = "AbandonedPageLoadMetricsObserver";
  return kName;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AbandonedPageLoadMetricsObserver::OnNavigationEvent(
    content::NavigationHandle* navigation_handle) {
  return CONTINUE_OBSERVING;
}

bool AbandonedPageLoadMetricsObserver::IsAllowedToLogMetrics() const {
  return true;
}

std::string AbandonedPageLoadMetricsObserver::GetHistogramPrefix() const {
  return internal::kAbandonedPageLoadMetricsHistogramPrefix;
}

std::vector<std::string>
AbandonedPageLoadMetricsObserver::GetAdditionalSuffixes() const {
  return {""};
}

std::string AbandonedPageLoadMetricsObserver::GetHistogramSuffix(
    NavigationMilestone milestone,
    base::TimeTicks event_time) const {
  std::string suffix = "";
  // If necessary, add suffixes to the histogram that indicates the event
  // happens after hiding/backgrounding. Note that for NavigationStart events,
  // the `event_time` is actually set to the current time / the time when we
  // first log all the milestones, so we explicitly skip that case (hiding or
  // backgrounding the navigation before it starts shouldn't count anyways).
  if (milestone != NavigationMilestone::kNavigationStart) {
    if (WasBackgrounded() && event_time > first_backgrounded_timestamp_) {
      suffix += internal::kSuffixWasBackgrounded;
    }
    if (WasHidden() && event_time > first_hidden_timestamp_) {
      suffix += internal::kSuffixWasHidden;
    }
  }

  return suffix;
}

std::string AbandonedPageLoadMetricsObserver::
    GetMilestoneToAbandonHistogramNameWithoutPrefixSuffix(
        NavigationMilestone milestone,
        std::optional<AbandonReason> abandon_reason) {
  return NavigationMilestoneToString(milestone) + "ToAbandon." +
         (abandon_reason.has_value()
              ? AbandonReasonToString(abandon_reason.value())
              : "");
}

std::string AbandonedPageLoadMetricsObserver::
    GetAbandonReasonAtMilestoneHistogramNameWithoutPrefixSuffix(
        NavigationMilestone milestone) {
  return std::string("AbandonReasonAt.") +
         NavigationMilestoneToString(milestone);
}

std::string AbandonedPageLoadMetricsObserver::
    GetLastMilestoneBeforeAbandonHistogramNameWithoutPrefixSuffix(
        std::optional<AbandonReason> abandon_reason) {
  return std::string("LastMilestoneBeforeAbandon.") +
         (abandon_reason.has_value()
              ? AbandonReasonToString(abandon_reason.value())
              : "");
}

std::string
AbandonedPageLoadMetricsObserver::GetMilestoneHistogramNameWithoutPrefixSuffix(
    NavigationMilestone milestone) {
  if (milestone == NavigationMilestone::kNavigationStart) {
    return std::string(internal::kMilestoneNavigationStart);
  }
  return std::string(internal::kMilestoneNavigationStart) + "To" +
         NavigationMilestoneToString(milestone);
}

void AbandonedPageLoadMetricsObserver::LogMilestoneHistogram(
    NavigationMilestone milestone,
    base::TimeTicks event_time,
    base::TimeTicks relative_start_time) {
  std::string base_suffix = GetHistogramSuffix(milestone, event_time);
  for (std::string additional_suffix : GetAdditionalSuffixes()) {
    std::string suffix = base_suffix + additional_suffix;
    PAGE_LOAD_HISTOGRAM(
        GetHistogramPrefix() +
            GetMilestoneHistogramNameWithoutPrefixSuffix(milestone) + suffix,
        event_time - relative_start_time);
  }
}

void AbandonedPageLoadMetricsObserver::LogMilestoneHistogram(
    NavigationMilestone milestone,
    base::TimeDelta event_time) {
  const base::TimeTicks navigation_start_time =
      GetDelegate().GetNavigationStart();
  LogMilestoneHistogram(milestone, navigation_start_time + event_time,
                        navigation_start_time);
}

void AbandonedPageLoadMetricsObserver::LogAbandonHistograms(
    AbandonReason abandon_reason,
    NavigationMilestone milestone,
    base::TimeTicks event_time,
    base::TimeTicks relative_start_time) {
  std::string base_suffix = GetHistogramSuffix(milestone, event_time);
  for (std::string additional_suffix : GetAdditionalSuffixes()) {
    std::string suffix = base_suffix + additional_suffix;
    PAGE_LOAD_HISTOGRAM(
        GetHistogramPrefix() +
            GetMilestoneToAbandonHistogramNameWithoutPrefixSuffix(
                milestone, abandon_reason) +
            suffix,
        event_time - relative_start_time);
    std::string milestone_string = NavigationMilestoneToString(milestone);
    base::UmaHistogramEnumeration(
        GetHistogramPrefix() +
            GetAbandonReasonAtMilestoneHistogramNameWithoutPrefixSuffix(
                milestone) +
            suffix,
        abandon_reason);
    base::UmaHistogramEnumeration(
        GetHistogramPrefix() +
            GetLastMilestoneBeforeAbandonHistogramNameWithoutPrefixSuffix(
                abandon_reason) +
            suffix,
        milestone);
  }
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AbandonedPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  page_load_metrics::PageLoadMetricsObserver::ObservePolicy
      navigation_handling_result = OnNavigationEvent(navigation_handle);
  if (navigation_handling_result != CONTINUE_OBSERVING) {
    return navigation_handling_result;
  }

  if (!started_in_foreground) {
    page_load_metrics::mojom::PageLoadTiming empty_timing;
    FlushMetricsOnAppEnterBackground(empty_timing);
  }

  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AbandonedPageLoadMetricsObserver::OnRedirect(
    content::NavigationHandle* navigation_handle) {
  page_load_metrics::PageLoadMetricsObserver::ObservePolicy
      navigation_handling_result = OnNavigationEvent(navigation_handle);
  if (navigation_handling_result != CONTINUE_OBSERVING) {
    return navigation_handling_result;
  }

  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AbandonedPageLoadMetricsObserver::OnNavigationHandleTimingUpdated(
    content::NavigationHandle* navigation_handle) {
  // Save the latest NavigationHandleTiming update, but don't log it right now.
  // It will be logged when the navigation commits or gets abandoned.
  latest_navigation_handle_timing_ =
      navigation_handle->GetNavigationHandleTiming();

  page_load_metrics::PageLoadMetricsObserver::ObservePolicy
      navigation_handling_result = OnNavigationEvent(navigation_handle);
  if (navigation_handling_result != CONTINUE_OBSERVING) {
    return navigation_handling_result;
  }

  if (navigation_handle->GetNetErrorCode() != net::OK) {
    // The navigation will commit an error page instead of the intended URL.
    // Record this as an abandonment as soon as we notice.
    if (IsAllowedToLogMetrics()) {
      LogMetricsOnAbandon(
          AbandonReason::kErrorPage,
          navigation_handle->GetNavigationHandleTiming().request_failed_time);
    }
    return STOP_OBSERVING;
  }

  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AbandonedPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  page_load_metrics::PageLoadMetricsObserver::ObservePolicy
      navigation_handling_result = OnNavigationEvent(navigation_handle);
  if (navigation_handling_result != CONTINUE_OBSERVING) {
    return navigation_handling_result;
  }

  // If the navigation has committed and it's still not allowed to log metrics,
  // then it will never log metrics, as the navigation details won't change
  // after this.
  if (!IsAllowedToLogMetrics()) {
    return STOP_OBSERVING;
  }

  LogNavigationMilestoneMetrics();
  LogMilestoneHistogram(NavigationMilestone::kDidCommit, base::TimeTicks::Now(),
                        GetDelegate().GetNavigationStart());

  // If there's any previous hiding/backgrounding that hasn't been logged (e.g.
  // if the navigation didn't allow logging when these abandonments happen),
  // log them now.
  LogPreviousHidingIfNeeded();
  LogPreviousBackgroundingIfNeeded();

  return CONTINUE_OBSERVING;
}

void AbandonedPageLoadMetricsObserver::OnParseStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  auto loading_milestone =
      std::make_pair(NavigationMilestone::kParseStart,
                     timing.parse_timing->parse_start.value());
  LogMilestoneHistogram(loading_milestone.first, loading_milestone.second);
  latest_loading_milestone_ = loading_milestone;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AbandonedPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // Prerender navigations won't be tracked.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AbandonedPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This class is interested only in events that are preprocessed and
  // dispatched also to the outermost page at PageLoadTracker. So, this class
  // doesn't need to forward events for FencedFrames.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AbandonedPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (first_backgrounded_timestamp_.is_null()) {
    first_backgrounded_timestamp_ = base::TimeTicks::Now();

    // This is the first time we're getting backgrounded. If we're allow to log
    // metrics, log the abandonment now.
    if (IsAllowedToLogMetrics()) {
      LogMetricsOnAbandon(AbandonReason::kAppBackgrounded,
                          first_backgrounded_timestamp_);
      did_log_backgrounding_ = true;
    }

    // Otherwise, we've saved the timestamp when we're first backgrounded, so if
    // the navigation eventually is allowed to log metrics and we log the
    // milestone metrics, we can note that this abandonment happened.
  }
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AbandonedPageLoadMetricsObserver::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (first_hidden_timestamp_.is_null()) {
    first_hidden_timestamp_ = base::TimeTicks::Now();

    // This is the first time we're getting hidden. If we're allow to log
    // metrics, log the abandonment now.
    if (IsAllowedToLogMetrics()) {
      LogMetricsOnAbandon(AbandonReason::kHidden, first_hidden_timestamp_);
      did_log_hiding_ = true;
    }

    // Otherwise, we've saved the timestamp when we're first hidden,  so if
    // the navigation eventually is allowed to log metrics and we log the
    // milestone metrics, we can note that this abandonment happened.
  }
  return CONTINUE_OBSERVING;
}

void AbandonedPageLoadMetricsObserver::LogPreviousHidingIfNeeded() {
  CHECK(IsAllowedToLogMetrics());
  if (WasHidden() && !did_log_hiding_) {
    LogMetricsOnAbandon(AbandonReason::kHidden, first_hidden_timestamp_);
    did_log_hiding_ = true;
  }
}
void AbandonedPageLoadMetricsObserver::LogPreviousBackgroundingIfNeeded() {
  CHECK(IsAllowedToLogMetrics());
  if (WasBackgrounded() && !did_log_backgrounding_) {
    LogMetricsOnAbandon(AbandonReason::kAppBackgrounded,
                        first_backgrounded_timestamp_);
    did_log_backgrounding_ = true;
  }
}

void AbandonedPageLoadMetricsObserver::OnFailedProvisionalLoad(
    const page_load_metrics::FailedProvisionalLoadInfo&
        failed_provisional_load_info) {
  if (!IsAllowedToLogMetrics()) {
    return;
  }
  LogMetricsOnAbandon(
      DiscardReasonToAbandonReason(failed_provisional_load_info.discard_reason),
      base::TimeTicks::Now());
}

void AbandonedPageLoadMetricsObserver::OnDidInternalNavigationAbort(
    content::NavigationHandle* navigation_handle) {
  if (!IsAllowedToLogMetrics()) {
    return;
  }
  CHECK(navigation_handle->GetNavigationDiscardReason().has_value());
  LogMetricsOnAbandon(
      DiscardReasonToAbandonReason(
          navigation_handle->GetNavigationDiscardReason().value()),
      base::TimeTicks::Now());
}

void AbandonedPageLoadMetricsObserver::LogMetricsOnAbandon(
    AbandonReason abandon_reason,
    base::TimeTicks abandon_timing) {
  CHECK(IsAllowedToLogMetrics());
  // We only log abandonments once and stop observing after abandonment, except
  // if the abandonment was because of backgrounding or hiding, in which case we
  // would continue observing and logging, but mark the logged metrics
  // specially.
  CHECK(!did_abandon_navigation_ || WasBackgrounded() || WasHidden());

  // Log the milestones first before logging any abandonment.
  LogNavigationMilestoneMetrics();

  // If the navigation was previously hidden or backgrounded and we haven't
  // logged them as abandonments (e.g. if the navigation wasn't allowed to log
  // metrics previously when those abandonments happened), log them first,
  // before logging this new abandonment.
  if (abandon_reason != AbandonReason::kHidden) {
    LogPreviousHidingIfNeeded();
  }
  if (abandon_reason != AbandonReason::kAppBackgrounded) {
    LogPreviousBackgroundingIfNeeded();
  }

  const std::string abandon_string = AbandonReasonToString(abandon_reason);

  // Log the time from the latest navigation or loading milestone received. This
  // helps us know at what point of the navigation the abandonment happened.
  // Note that for redirects and non-redirects we only check "loader callback"
  // milestones and not the "response start" or "request start" counterparts,
  // since we're only notified of NavigationHandleTiming update when we get the
  // loader callback. Thus, the loader callback timing must be more recent than
  // the response start or request start counterpart.
  if (latest_loading_milestone_.has_value() &&
      abandon_timing > latest_loading_milestone_->second +
                           GetDelegate().GetNavigationStart()) {
    // `latest_loading_milestone_` has the taken time from the navigation start
    // time as base::TimeDelta, adding the navigation start time to measure the
    // abandoned time.
    LogAbandonHistograms(
        abandon_reason, latest_loading_milestone_->first, abandon_timing,
        latest_loading_milestone_->second + GetDelegate().GetNavigationStart());
  } else if (!latest_navigation_handle_timing_.navigation_commit_sent_time
                  .is_null() &&
             abandon_timing >
                 latest_navigation_handle_timing_.navigation_commit_sent_time) {
    LogAbandonHistograms(
        abandon_reason, NavigationMilestone::kCommitSent, abandon_timing,
        latest_navigation_handle_timing_.navigation_commit_sent_time);
  } else if (!latest_navigation_handle_timing_
                  .non_redirect_response_loader_callback_time.is_null() &&
             abandon_timing > latest_navigation_handle_timing_
                                  .non_redirect_response_loader_callback_time) {
    LogAbandonHistograms(
        abandon_reason, NavigationMilestone::kNonRedirectResponseLoaderCallback,
        abandon_timing,
        latest_navigation_handle_timing_
            .non_redirect_response_loader_callback_time);
  } else if (!latest_navigation_handle_timing_.first_loader_callback_time
                  .is_null() &&
             abandon_timing >
                 latest_navigation_handle_timing_.first_loader_callback_time) {
    LogAbandonHistograms(
        abandon_reason,
        NavigationMilestone::kFirstRedirectResponseLoaderCallback,
        abandon_timing,
        latest_navigation_handle_timing_.first_loader_callback_time);
  } else if (!latest_navigation_handle_timing_.loader_start_time.is_null() &&
             abandon_timing >
                 latest_navigation_handle_timing_.loader_start_time) {
    LogAbandonHistograms(abandon_reason, NavigationMilestone::kLoaderStart,
                         abandon_timing,
                         latest_navigation_handle_timing_.loader_start_time);
  } else {
    LogAbandonHistograms(abandon_reason, NavigationMilestone::kNavigationStart,
                         abandon_timing, GetDelegate().GetNavigationStart());
  }
}

void AbandonedPageLoadMetricsObserver::LogNavigationMilestoneMetrics() {
  CHECK(IsAllowedToLogMetrics());
  CHECK(!did_abandon_navigation_ || WasBackgrounded() || WasHidden());

  const base::TimeTicks navigation_start_time =
      GetDelegate().GetNavigationStart();
  if (!did_log_navigation_start_) {
    // Log NavigationStart exactly once.
    LogMilestoneHistogram(NavigationMilestone::kNavigationStart,
                          base::TimeTicks::Now(), navigation_start_time);
    did_log_navigation_start_ = true;
  }

  // Log the latest timings from `latest_navigation_handle_timing_` and save the
  // logged timings to `last_logged_navigation_handle_timing_`. We track these
  // separately since we might call this function multiple times, and we want to
  // ensure each milestone is only logged once per navigation.
  if (!latest_navigation_handle_timing_.navigation_commit_sent_time.is_null() &&
      last_logged_navigation_handle_timing_.navigation_commit_sent_time
          .is_null()) {
    LogMilestoneHistogram(
        NavigationMilestone::kCommitSent,
        latest_navigation_handle_timing_.navigation_commit_sent_time,
        navigation_start_time);
  }

  if (!latest_navigation_handle_timing_
           .non_redirect_response_loader_callback_time.is_null() &&
      last_logged_navigation_handle_timing_
          .non_redirect_response_loader_callback_time.is_null()) {
    // The navigation had received its final non-redirect response.
    LogMilestoneHistogram(
        NavigationMilestone::kNonRedirectResponseLoaderCallback,
        latest_navigation_handle_timing_
            .non_redirect_response_loader_callback_time,
        navigation_start_time);
    if (!latest_navigation_handle_timing_.non_redirect_response_start_time
             .is_null()) {
      LogMilestoneHistogram(
          NavigationMilestone::kNonRedirectResponseStart,
          latest_navigation_handle_timing_.non_redirect_response_start_time,
          navigation_start_time);
    }
    if (!latest_navigation_handle_timing_.non_redirected_request_start_time
             .is_null()) {
      LogMilestoneHistogram(
          NavigationMilestone::kNonRedirectedRequestStart,
          latest_navigation_handle_timing_.non_redirected_request_start_time,
          navigation_start_time);
    }
  }

  if (!latest_navigation_handle_timing_.first_loader_callback_time.is_null() &&
      last_logged_navigation_handle_timing_.first_loader_callback_time
          .is_null() &&
      latest_navigation_handle_timing_.first_loader_callback_time !=
          latest_navigation_handle_timing_
              .non_redirect_response_loader_callback_time) {
    // If we got a response that is not the final response, it must be a
    // redirect response.
    LogMilestoneHistogram(
        NavigationMilestone::kFirstRedirectResponseLoaderCallback,
        latest_navigation_handle_timing_.first_loader_callback_time,
        navigation_start_time);
    if (!latest_navigation_handle_timing_.first_response_start_time.is_null()) {
      LogMilestoneHistogram(
          NavigationMilestone::kFirstRedirectResponseStart,
          latest_navigation_handle_timing_.first_response_start_time,
          navigation_start_time);
    }

    if (!latest_navigation_handle_timing_.first_request_start_time.is_null()) {
      LogMilestoneHistogram(
          NavigationMilestone::kFirstRedirectedRequestStart,
          latest_navigation_handle_timing_.first_request_start_time,
          navigation_start_time);
    }
  }

  if (!latest_navigation_handle_timing_.loader_start_time.is_null() &&
      last_logged_navigation_handle_timing_.loader_start_time.is_null()) {
    LogMilestoneHistogram(NavigationMilestone::kLoaderStart,
                          latest_navigation_handle_timing_.loader_start_time,
                          navigation_start_time);
  }

  last_logged_navigation_handle_timing_ = latest_navigation_handle_timing_;
}
