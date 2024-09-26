// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/abandoned_page_load_metrics_observer.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

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
    case content::NavigationDiscardReason::kNewDuplicateNavigation:
      return AbandonReason::kNewDuplicateNavigation;
      // Other cases like kCommittedNavigation and kRenderFrameHostDestruction
      // should be obsolete, so just use "other" as the reason.
    case content::NavigationDiscardReason::kCommittedNavigation:
    case content::NavigationDiscardReason::kRenderFrameHostDestruction:
      return AbandonReason::kOther;
  }
}

bool IsEventAfter(base::TimeTicks event_time, base::TimeTicks time_to_compare) {
  return !time_to_compare.is_null() && event_time > time_to_compare;
}

}  // namespace

namespace internal {

const char kAbandonedPageLoadMetricsHistogramPrefix[] =
    "PageLoad.Clients.Leakage2.";

const char kMilestoneToAbandon[] = "ToAbandon";
const char kLastMilestoneBeforeAbandon[] = "LastMilestoneBeforeAbandon";
const char kTimeToAbandonFromNavigationStart[] =
    "TimeToAbandonFromNavigationStart.";

const char kAbandonReasonNewReloadNavigation[] = "NewReloadNavigation";
const char kAbandonReasonNewHistoryNavigation[] = "NewHistoryNavigation";
const char kAbandonReasonNewOtherNavigationBrowserInitiated[] =
    "NewOtherNavigationBrowserInitiated";
const char kAbandonReasonNewOtherNavigationRendererInitiated[] =
    "NewOtherNavigationRendererInitiated";
const char kAbandonReasonNewDuplicateNavigation[] = "NewDuplicateNavigation";
const char kAbandonReasonFrameRemoved[] = "FrameRemoved";
const char kAbandonReasonExplicitCancellation[] = "ExplicitCancellation";
const char kAbandonReasonInternalCancellation[] = "InternalCancellation";
const char kAbandonReasonRendererProcessGone[] = "RendererProcessGone";
const char kAbandonReasonNeverStarted[] = "NeverStarted";
const char kAbandonReasonFailedSecurityCheck[] = "FailedSecurityCheck";
const char kAbandonReasonOther[] = "Other";
const char kAbandonReasonHidden[] = "Hidden";
const char kAbandonReasonErrorPage[] = "ErrorPage";
const char kAbandonReasonAppBackgrounded[] = "AppBackgrounded";

const char kSuffixWasAppBackgrounded[] = ".WasAppBackgrounded";
const char kSuffixTabWasHiddenAtStartStaysHidden[] =
    ".TabWasHiddenAtStartStaysHidden";
const char kSuffixTabWasHiddenAtStartLaterShown[] =
    ".TabWasHiddenAtStartLaterShown";
const char kSuffixTabWasHiddenStaysHidden[] = ".TabWasHiddenStaysHidden";
const char kSuffixTabWasHiddenLaterShown[] = ".TabWasHiddenLaterShown";

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
const char kMilestoneCommitReceived[] = "CommitReceived";
const char kMilestoneDidCommit[] = "DidCommit";
const char kMilestoneParseStart[] = "ParseStart";
const char kFirstContentfulPaint[] = "FirstContentfulPaint";
const char kDOMContentLoaded[] = "DOMContentLoaded";
const char kLoadEventStarted[] = "LoadStarted";
const char kLargestContentfulPaint[] = "LargestContentfulPaint";

const char kAFTStart[] = "AFTStart";
const char kAFTEnd[] = "AFTEnd";
const char kHeaderChunkStart[] = "HeaderChunkStart";
const char kHeaderChunkEnd[] = "HeaderChunkEnd";
const char kBodyChunkStart[] = "BodyChunkStart";
const char kBodyChunkEnd[] = "BodyChunkEnd";

const char kRendererProcessCreatedBeforeNavHistogramName[] =
    "RendererProcessCreatedBeforeNav";
const char kRendererProcessInitHistogramName[] =
    "NavigationStartToRendererProcessInit";

const char kNavigationTypeHistoryNav[] = "HistoryNav";
const char kNavigationTypeReloadNav[] = "ReloadNav";
const char kNavigationTypeRestoreNav[] = "RestoreNav";
const char kNavigationTypeBrowserNav[] = "BrowserNav";
const char kNavigationTypeRendererNav[] = "RendererNav";

const char kSuffixAbandonedTimeBelow100[] = ".AbandonedTimeBelow100";
const char kSuffixAbandonedTime100to2000[] = ".AbandonedTime100To2000";
const char kSuffixAbandonedTimeAbove2000[] = ".AbandonedTimeAbove2000";

const char* GetSuffixForAbandonedTime(base::TimeDelta request_failed_time) {
  if (request_failed_time.InMilliseconds() < 100) {
    return internal::kSuffixAbandonedTimeBelow100;
  }
  if (request_failed_time.InMilliseconds() <= 2000) {
    return internal::kSuffixAbandonedTime100to2000;
  }

  return internal::kSuffixAbandonedTimeAbove2000;
}

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
    case AbandonReason::kNewDuplicateNavigation:
      return internal::kAbandonReasonNewDuplicateNavigation;
    case AbandonReason::kFrameRemoved:
      return internal::kAbandonReasonFrameRemoved;
    case AbandonReason::kExplicitCancellation:
      return internal::kAbandonReasonExplicitCancellation;
    case AbandonReason::kInternalCancellation:
      return internal::kAbandonReasonInternalCancellation;
    case AbandonReason::kRenderProcessGone:
      return internal::kAbandonReasonRendererProcessGone;
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
    case NavigationMilestone::kCommitReceived:
      return internal::kMilestoneCommitReceived;
    case NavigationMilestone::kDidCommit:
      return internal::kMilestoneDidCommit;
    case NavigationMilestone::kParseStart:
      return internal::kMilestoneParseStart;
    case NavigationMilestone::kFirstContentfulPaint:
      return internal::kFirstContentfulPaint;
    case NavigationMilestone::kDOMContentLoaded:
      return internal::kDOMContentLoaded;
    case NavigationMilestone::kLoadEventStarted:
      return internal::kLoadEventStarted;
    case NavigationMilestone::kLargestContentfulPaint:
      return internal::kLargestContentfulPaint;
    case NavigationMilestone::kAFTStart:
      return internal::kAFTStart;
    case NavigationMilestone::kAFTEnd:
      return internal::kAFTEnd;
    case NavigationMilestone::kHeaderChunkStart:
      return internal::kHeaderChunkStart;
    case NavigationMilestone::kHeaderChunkEnd:
      return internal::kHeaderChunkEnd;
    case NavigationMilestone::kBodyChunkStart:
      return internal::kBodyChunkStart;
    case NavigationMilestone::kBodyChunkEnd:
      return internal::kBodyChunkEnd;
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

bool AbandonedPageLoadMetricsObserver::IsAllowedToLogUKM() const {
  // UKM logging is not triggered by default, to avoid hitting the entry limit.
  return false;
}

std::string AbandonedPageLoadMetricsObserver::GetHistogramPrefix() const {
  return internal::kAbandonedPageLoadMetricsHistogramPrefix;
}

std::vector<std::string>
AbandonedPageLoadMetricsObserver::GetAdditionalSuffixes() const {
  return {""};
}

const base::flat_map<std::string,
                     AbandonedPageLoadMetricsObserver::NavigationMilestone>&
AbandonedPageLoadMetricsObserver::GetCustomUserTimingMarkNames() const {
  static const base::NoDestructor<
      base::flat_map<std::string, NavigationMilestone>>
      mark_names;
  return *mark_names;
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
      suffix += internal::kSuffixWasAppBackgrounded;
    }

    if (!started_in_foreground_) {
      if (!first_shown_timestamp_.is_null() &&
          event_time > first_shown_timestamp_) {
        suffix += internal::kSuffixTabWasHiddenAtStartLaterShown;
      } else {
        suffix += internal::kSuffixTabWasHiddenAtStartStaysHidden;
      }
    } else if (WasHidden() && event_time > first_hidden_timestamp_) {
      if (!last_shown_timestamp_.is_null() &&
          event_time > last_shown_timestamp_ &&
          last_shown_timestamp_ > first_hidden_timestamp_) {
        // Hidden after navigation start, then shown.
        suffix += internal::kSuffixTabWasHiddenLaterShown;
      } else {
        // Stays hidden.
        suffix += internal::kSuffixTabWasHiddenStaysHidden;
      }
    }
  }

  return suffix;
}

std::string AbandonedPageLoadMetricsObserver::
    GetMilestoneToAbandonHistogramNameWithoutPrefixSuffix(
        NavigationMilestone milestone,
        std::optional<AbandonReason> abandon_reason) {
  const std::string milestone_to_abandon = base::StrCat(
      {NavigationMilestoneToString(milestone), internal::kMilestoneToAbandon});
  if (abandon_reason.has_value()) {
    return base::StrCat({milestone_to_abandon, ".",
                         AbandonReasonToString(abandon_reason.value())});
  }
  return milestone_to_abandon;
}

std::string AbandonedPageLoadMetricsObserver::
    GetAbandonReasonAtMilestoneHistogramNameWithoutPrefixSuffix(
        NavigationMilestone milestone) {
  return base::StrCat(
      {"AbandonReasonAt.", NavigationMilestoneToString(milestone)});
}

std::string AbandonedPageLoadMetricsObserver::
    GetLastMilestoneBeforeAbandonHistogramNameWithoutPrefixSuffix(
        std::optional<AbandonReason> abandon_reason) {
  if (abandon_reason.has_value()) {
    return base::StrCat(
        {std::string_view(internal::kLastMilestoneBeforeAbandon), ".",
         AbandonReasonToString(abandon_reason.value())});
  }
  return internal::kLastMilestoneBeforeAbandon;
}

std::string
AbandonedPageLoadMetricsObserver::GetMilestoneHistogramNameWithoutPrefixSuffix(
    NavigationMilestone milestone) {
  if (milestone == NavigationMilestone::kNavigationStart) {
    return std::string(internal::kMilestoneNavigationStart);
  }
  return base::StrCat({std::string_view(internal::kMilestoneNavigationStart),
                       "To", NavigationMilestoneToString(milestone)});
}

std::string AbandonedPageLoadMetricsObserver::
    GetTimeToAbandonFromNavigationStartWithoutPrefixSuffix(
        NavigationMilestone milestone) {
  return base::StrCat({internal::kTimeToAbandonFromNavigationStart,
                       NavigationMilestoneToString(milestone)});
}

std::string
AbandonedPageLoadMetricsObserver::GetNavigationTypeToAbandonWithoutPrefixSuffix(
    const std::string_view& tracked_navigation_type,
    std::optional<AbandonReason> abandon_reason) {
  return base::StrCat({tracked_navigation_type, "To",
                       (abandon_reason.has_value()
                            ? AbandonReasonToString(abandon_reason.value())
                            : "")});
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
  LogMilestoneHistogram(milestone, navigation_start_time_ + event_time,
                        navigation_start_time_);
}

void AbandonedPageLoadMetricsObserver::LogAbandonHistograms(
    AbandonReason abandon_reason,
    NavigationMilestone milestone,
    base::TimeTicks event_time,
    base::TimeTicks relative_start_time) {
  std::string base_suffix = GetHistogramSuffix(milestone, event_time);
  for (std::string additional_suffix : GetAdditionalSuffixes()) {
    std::string suffix = base::StrCat({base_suffix, additional_suffix});
    PAGE_LOAD_HISTOGRAM(
        base::StrCat({GetHistogramPrefix(),
                      GetMilestoneToAbandonHistogramNameWithoutPrefixSuffix(
                          milestone, abandon_reason),
                      suffix}),
        event_time - relative_start_time);
    PAGE_LOAD_HISTOGRAM(
        base::StrCat({GetHistogramPrefix(),
                      GetMilestoneToAbandonHistogramNameWithoutPrefixSuffix(
                          milestone, std::nullopt),
                      suffix}),
        event_time - relative_start_time);
    PAGE_LOAD_HISTOGRAM(
        base::StrCat(
            {GetHistogramPrefix(),
             GetTimeToAbandonFromNavigationStartWithoutPrefixSuffix(milestone),
             suffix}),
        event_time - navigation_start_time_);
    PAGE_LOAD_HISTOGRAM(
        base::StrCat({GetHistogramPrefix(),
                      GetNavigationTypeToAbandonWithoutPrefixSuffix(
                          navigation_type_, abandon_reason),
                      suffix}),
        event_time - navigation_start_time_);
    base::UmaHistogramEnumeration(
        base::StrCat(
            {GetHistogramPrefix(),
             GetAbandonReasonAtMilestoneHistogramNameWithoutPrefixSuffix(
                 milestone),
             suffix}),
        abandon_reason);
    base::UmaHistogramEnumeration(
        base::StrCat(
            {GetHistogramPrefix(),
             GetLastMilestoneBeforeAbandonHistogramNameWithoutPrefixSuffix(
                 abandon_reason),
             suffix}),
        milestone);
    base::UmaHistogramEnumeration(
        base::StrCat(
            {GetHistogramPrefix(),
             GetLastMilestoneBeforeAbandonHistogramNameWithoutPrefixSuffix(
                 std::nullopt),
             suffix}),
        milestone);
  }

  if (!IsAllowedToLogUKM()) {
    return;
  }

  ukm::SourceId source_id =
      ukm::ConvertToSourceId(navigation_id_, ukm::SourceIdType::NAVIGATION_ID);
  ukm::builders::AbandonedSRPNavigation builder(source_id);
  builder.SetAbandonReason(static_cast<int>(abandon_reason));
  builder.SetLastMilestoneBeforeAbandon(static_cast<int>(milestone));

  builder.SetAbandonTimingFromNavigationStart(
      (event_time - navigation_start_time_).InMilliseconds());
  builder.SetAbandonTimingFromLastMilestone(
      (event_time - relative_start_time).InMilliseconds());
  if (IsEventAfter(event_time, first_backgrounded_timestamp_)) {
    builder.SetPreviousBackgroundedTime(
        (first_backgrounded_timestamp_ - navigation_start_time_)
            .InMilliseconds());
  }

  if (IsEventAfter(event_time, first_hidden_timestamp_)) {
    builder.SetPreviousHiddenTime(
        (first_hidden_timestamp_ - navigation_start_time_).InMilliseconds());
  }

  if (IsEventAfter(event_time, renderer_process_init_time_)) {
    builder.SetRendererProcessInitTime(
        (renderer_process_init_time_ - navigation_start_time_)
            .InMilliseconds());
  }

  if (IsEventAfter(event_time,
                   latest_navigation_handle_timing_.loader_start_time)) {
    builder.SetLoaderStartTime(
        (latest_navigation_handle_timing_.loader_start_time -
         navigation_start_time_)
            .InMilliseconds());
  }

  if (latest_navigation_handle_timing_.first_loader_callback_time !=
          latest_navigation_handle_timing_
              .non_redirect_response_loader_callback_time &&
      IsEventAfter(
          event_time,
          latest_navigation_handle_timing_.first_loader_callback_time)) {
    builder.SetFirstRedirectResponseReceived(true);
    if (IsEventAfter(
            event_time,
            latest_navigation_handle_timing_.first_request_start_time)) {
      builder.SetFirstRedirectedRequestStartTime(
          (latest_navigation_handle_timing_.first_request_start_time -
           navigation_start_time_)
              .InMilliseconds());
    }
  } else {
    builder.SetFirstRedirectResponseReceived(false);
  }

  builder.SetNonRedirectResponseReceived(
      !latest_navigation_handle_timing_
           .non_redirect_response_loader_callback_time.is_null());
  if (IsEventAfter(
          event_time,
          latest_navigation_handle_timing_.non_redirected_request_start_time)) {
    builder.SetNonRedirectedRequestStartTime(
        (latest_navigation_handle_timing_.non_redirected_request_start_time -
         navigation_start_time_)
            .InMilliseconds());
  }

  if (IsEventAfter(
          event_time,
          latest_navigation_handle_timing_.navigation_commit_sent_time)) {
    builder.SetCommitSentTime(
        (latest_navigation_handle_timing_.navigation_commit_sent_time -
         navigation_start_time_)
            .InMilliseconds());
  }

  if (IsEventAfter(
          event_time,
          latest_navigation_handle_timing_.navigation_commit_received_time)) {
    builder.SetCommitReceivedTime(
        (latest_navigation_handle_timing_.navigation_commit_received_time -
         navigation_start_time_)
            .InMilliseconds());
  }

  if (IsEventAfter(
          event_time,
          latest_navigation_handle_timing_.navigation_did_commit_time)) {
    builder.SetDidCommitTime(
        (latest_navigation_handle_timing_.navigation_did_commit_time -
         navigation_start_time_)
            .InMilliseconds());
  }

  for (const auto& loading_milestone : loading_milestones_) {
    if (!IsEventAfter(event_time,
                      loading_milestone.second + navigation_start_time_)) {
      continue;
    }
    auto time = loading_milestone.second.InMilliseconds();
    switch (loading_milestone.first) {
      case NavigationMilestone::kParseStart:
        builder.SetParseStartTime(time);
        break;
      case NavigationMilestone::kFirstContentfulPaint:
        builder.SetFirstContentfulPaintTime(time);
        break;
      case NavigationMilestone::kDOMContentLoaded:
        builder.SetDOMContentLoadedTime(time);
        break;
      case NavigationMilestone::kLoadEventStarted:
        builder.SetLoadEventStartedTime(time);
        break;
      case NavigationMilestone::kLargestContentfulPaint:
        builder.SetLargestContentfulPaintTime(time);
        break;
      case NavigationMilestone::kAFTStart:
        builder.SetAFTStartTime(time);
        break;
      case NavigationMilestone::kAFTEnd:
        builder.SetAFTEndTime(time);
        break;
      case NavigationMilestone::kHeaderChunkStart:
        builder.SetHeaderChunkStartTime(time);
        break;
      case NavigationMilestone::kHeaderChunkEnd:
        builder.SetHeaderChunkEndTime(time);
        break;
      case NavigationMilestone::kBodyChunkStart:
        builder.SetBodyChunkStartTime(time);
        break;
      case NavigationMilestone::kBodyChunkEnd:
        builder.SetBodyChunkEndTime(time);
        break;
      case NavigationMilestone::kNavigationStart:
      case NavigationMilestone::kLoaderStart:
      case NavigationMilestone::kFirstRedirectedRequestStart:
      case NavigationMilestone::kFirstRedirectResponseStart:
      case NavigationMilestone::kFirstRedirectResponseLoaderCallback:
      case NavigationMilestone::kNonRedirectedRequestStart:
      case NavigationMilestone::kNonRedirectResponseStart:
      case NavigationMilestone::kNonRedirectResponseLoaderCallback:
      case NavigationMilestone::kCommitSent:
      case NavigationMilestone::kCommitReceived:
      case NavigationMilestone::kDidCommit:
        break;
    }
  }

  AddSRPMetricsToUKMIfNeeded(builder);

  builder.Record(ukm::UkmRecorder::Get());
}

void AbandonedPageLoadMetricsObserver::LogLoadingMilestone(
    NavigationMilestone milestone,
    base::TimeDelta time) {
  LogMilestoneHistogram(milestone, time);
  loading_milestones_.emplace_back(milestone, time);
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AbandonedPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  navigation_id_ = navigation_handle->GetNavigationId();
  navigation_start_time_ = GetDelegate().GetNavigationStart();
  started_in_foreground_ = started_in_foreground;
  if (navigation_handle->IsHistory()) {
    navigation_type_ = internal::kNavigationTypeHistoryNav;
  } else if (navigation_handle->GetReloadType() != content::ReloadType::NONE) {
    navigation_type_ = internal::kNavigationTypeReloadNav;
  } else if (navigation_handle->GetRestoreType() !=
             content::RestoreType::kNotRestored) {
    navigation_type_ = internal::kNavigationTypeRestoreNav;
  } else if (navigation_handle->IsRendererInitiated()) {
    navigation_type_ = internal::kNavigationTypeRendererNav;
  } else {
    navigation_type_ = internal::kNavigationTypeBrowserNav;
  }

  page_load_metrics::PageLoadMetricsObserver::ObservePolicy
      navigation_handling_result = OnNavigationEvent(navigation_handle);
  if (navigation_handling_result != CONTINUE_OBSERVING) {
    return navigation_handling_result;
  }

  if (!started_in_foreground) {
    OnHiddenInternal();
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

  if (renderer_process_init_time_.is_null() &&
      !latest_navigation_handle_timing_.navigation_commit_sent_time.is_null() &&
      navigation_handle->GetRenderFrameHost()) {
    renderer_process_init_time_ = navigation_handle->GetRenderFrameHost()
                                      ->GetProcess()
                                      ->GetLastInitTime();
    bool renderer_process_created_before_navigation =
        (renderer_process_init_time_ < navigation_start_time_);
    std::string base_suffix = GetHistogramSuffix(
        NavigationMilestone::kCommitSent, renderer_process_init_time_);
    for (std::string additional_suffix : GetAdditionalSuffixes()) {
      std::string suffix = base_suffix + additional_suffix;
      base::UmaHistogramBoolean(
          GetHistogramPrefix() +
              internal::kRendererProcessCreatedBeforeNavHistogramName + suffix,
          renderer_process_created_before_navigation);
      if (renderer_process_created_before_navigation) {
        continue;
      }
      PAGE_LOAD_HISTOGRAM(GetHistogramPrefix() +
                              internal::kRendererProcessInitHistogramName +
                              suffix,
                          renderer_process_init_time_ - navigation_start_time_);
    }
  }

  // Check if the response came from http cache or not.
  if (!latest_navigation_handle_timing_.non_redirect_response_start_time
           .is_null() &&
      latest_navigation_handle_timing_.navigation_commit_sent_time.is_null()) {
    was_cached_ = navigation_handle->WasResponseCached();
  }

  if (navigation_handle->GetNetErrorCode() != net::OK) {
    // The navigation will commit an error page instead of the intended URL.
    // Record this as an abandonment as soon as we notice.
    if (IsAllowedToLogMetrics()) {
      LogMetricsOnAbandon(
          AbandonReason::kErrorPage,
          navigation_handle->GetNavigationHandleTiming().request_failed_time);
      LogNetError(
          navigation_handle->GetNetErrorCode(),
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
  LogMilestoneHistogram(NavigationMilestone::kCommitReceived,
                        navigation_handle->GetNavigationHandleTiming()
                            .navigation_commit_received_time,
                        navigation_start_time_);
  LogMilestoneHistogram(
      NavigationMilestone::kDidCommit,
      navigation_handle->GetNavigationHandleTiming().navigation_did_commit_time,
      navigation_start_time_);

  // If there's any previous hiding/backgrounding that hasn't been logged (e.g.
  // if the navigation didn't allow logging when these abandonments happen),
  // log them now.
  LogPreviousHidingIfNeeded();
  LogPreviousBackgroundingIfNeeded();

  return CONTINUE_OBSERVING;
}

void AbandonedPageLoadMetricsObserver::OnParseStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  LogLoadingMilestone(NavigationMilestone::kParseStart,
                      timing.parse_timing->parse_start.value());
}

void AbandonedPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  LogLoadingMilestone(NavigationMilestone::kFirstContentfulPaint,
                      timing.paint_timing->first_contentful_paint.value());
}

void AbandonedPageLoadMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  LogLoadingMilestone(
      NavigationMilestone::kDOMContentLoaded,
      timing.document_timing->dom_content_loaded_event_start.value());
}

void AbandonedPageLoadMetricsObserver::OnLoadEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  LogLoadingMilestone(NavigationMilestone::kLoadEventStarted,
                      timing.document_timing->load_event_start.value());
}

void AbandonedPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  FinalizeLCP();
}

void AbandonedPageLoadMetricsObserver::OnCustomUserTimingMarkObserved(
    const std::vector<page_load_metrics::mojom::CustomUserTimingMarkPtr>&
        timings) {
  base::flat_map<std::string, NavigationMilestone> custom_timings =
      GetCustomUserTimingMarkNames();
  for (const auto& mark : timings) {
    if (custom_timings.contains(mark->mark_name)) {
      LogLoadingMilestone(custom_timings[mark->mark_name], mark->start_time);
    }
  }
}

void AbandonedPageLoadMetricsObserver::FinalizeLCP() {
  if (is_lcp_finalized_) {
    return;
  }
  const page_load_metrics::ContentfulPaintTimingInfo& largest_contentful_paint =
      GetDelegate()
          .GetLargestContentfulPaintHandler()
          .MergeMainFrameAndSubframes();
  if (largest_contentful_paint.ContainsValidTime() &&
      WasStartedInForegroundOptionalEventInForeground(
          largest_contentful_paint.Time(), GetDelegate())) {
    // LCP (PageLoad.PaintTiming.NavigationToLargestContentfulPaint2) is
    // recorded only when WasStartedInForegroundOptionalEventInForeground() is
    // true. The LCP milestone recorded here should be consistent with the
    // regular LCP condition. Otherwise it will be less reliable.
    LogLoadingMilestone(NavigationMilestone::kLargestContentfulPaint,
                        largest_contentful_paint.Time().value());
    is_lcp_finalized_ = true;
  }
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
  if (GetDelegate().DidCommit()) {
    FinalizeLCP();
  }

  if (first_backgrounded_timestamp_.is_null()) {
    first_backgrounded_timestamp_ = base::TimeTicks::Now();

    // This is the first time we're getting backgrounded. If we're allow to log
    // metrics, log the abandonment now.
    if (IsAllowedToLogMetrics()) {
      did_log_backgrounding_ = true;
      LogMetricsOnAbandon(AbandonReason::kAppBackgrounded,
                          first_backgrounded_timestamp_);
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
  OnHiddenInternal();
  return CONTINUE_OBSERVING;
}

void AbandonedPageLoadMetricsObserver::OnHiddenInternal() {
  if (first_hidden_timestamp_.is_null()) {
    first_hidden_timestamp_ = base::TimeTicks::Now();

    // This is the first time we're getting hidden. If we're allow to log
    // metrics, log the abandonment now.
    if (IsAllowedToLogMetrics()) {
      did_log_hiding_ = true;
      LogMetricsOnAbandon(AbandonReason::kHidden, first_hidden_timestamp_);
    }

    // Otherwise, we've saved the timestamp when we're first hidden,  so if
    // the navigation eventually is allowed to log metrics and we log the
    // milestone metrics, we can note that this abandonment happened.
  }
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AbandonedPageLoadMetricsObserver::OnShown() {
  if (first_shown_timestamp_.is_null()) {
    first_shown_timestamp_ = base::TimeTicks::Now();
  }
  last_shown_timestamp_ = base::TimeTicks::Now();
  return CONTINUE_OBSERVING;
}

void AbandonedPageLoadMetricsObserver::LogPreviousHidingIfNeeded() {
  CHECK(IsAllowedToLogMetrics());
  if (WasHidden() && !did_log_hiding_) {
    did_log_hiding_ = true;
    LogMetricsOnAbandon(AbandonReason::kHidden, first_hidden_timestamp_);
  }
}
void AbandonedPageLoadMetricsObserver::LogPreviousBackgroundingIfNeeded() {
  CHECK(IsAllowedToLogMetrics());
  if (WasBackgrounded() && !did_log_backgrounding_) {
    did_log_backgrounding_ = true;
    LogMetricsOnAbandon(AbandonReason::kAppBackgrounded,
                        first_backgrounded_timestamp_);
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

  // Find the most latest loading milestone before the loading is abandoned. If
  // found, log it as an abandoned milesone later.
  std::optional<LoadingMilestone> latest_loading_milestone_to_abandon;
  for (const auto& milestone : loading_milestones_) {
    if (abandon_timing <= milestone.second + navigation_start_time_) {
      continue;
    }
    if (!latest_loading_milestone_to_abandon.has_value() ||
        milestone.second > latest_loading_milestone_to_abandon->second) {
      latest_loading_milestone_to_abandon = milestone;
    }
  }

  // Log the time from the latest navigation or loading milestone received. This
  // helps us know at what point of the navigation the abandonment happened.
  // Note that for redirects and non-redirects we only check "loader callback"
  // milestones and not the "response start" or "request start" counterparts,
  // since we're only notified of NavigationHandleTiming update when we get the
  // loader callback. Thus, the loader callback timing must be more recent than
  // the response start or request start counterpart.
  if (latest_loading_milestone_to_abandon.has_value()) {
    // `latest_loading_milestone_to_abandon` has the taken time from the
    // navigation start time as base::TimeDelta, adding the navigation start
    // time to measure the abandoned time.
    LogAbandonHistograms(
        abandon_reason, latest_loading_milestone_to_abandon->first,
        abandon_timing,
        latest_loading_milestone_to_abandon->second + navigation_start_time_);
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
                         abandon_timing, navigation_start_time_);
  }
}

void AbandonedPageLoadMetricsObserver::LogNavigationMilestoneMetrics() {
  CHECK(IsAllowedToLogMetrics());
  CHECK(!did_abandon_navigation_ || WasBackgrounded() || WasHidden());

  if (!did_log_navigation_start_) {
    // Log NavigationStart exactly once.
    LogMilestoneHistogram(NavigationMilestone::kNavigationStart,
                          base::TimeTicks::Now(), navigation_start_time_);
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
        navigation_start_time_);
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
        navigation_start_time_);
    if (!latest_navigation_handle_timing_.non_redirect_response_start_time
             .is_null()) {
      LogMilestoneHistogram(
          NavigationMilestone::kNonRedirectResponseStart,
          latest_navigation_handle_timing_.non_redirect_response_start_time,
          navigation_start_time_);
    }
    if (!latest_navigation_handle_timing_.non_redirected_request_start_time
             .is_null()) {
      LogMilestoneHistogram(
          NavigationMilestone::kNonRedirectedRequestStart,
          latest_navigation_handle_timing_.non_redirected_request_start_time,
          navigation_start_time_);
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
        navigation_start_time_);
    if (!latest_navigation_handle_timing_.first_response_start_time.is_null()) {
      LogMilestoneHistogram(
          NavigationMilestone::kFirstRedirectResponseStart,
          latest_navigation_handle_timing_.first_response_start_time,
          navigation_start_time_);
    }

    if (!latest_navigation_handle_timing_.first_request_start_time.is_null()) {
      LogMilestoneHistogram(
          NavigationMilestone::kFirstRedirectedRequestStart,
          latest_navigation_handle_timing_.first_request_start_time,
          navigation_start_time_);
    }
  }

  if (!latest_navigation_handle_timing_.loader_start_time.is_null() &&
      last_logged_navigation_handle_timing_.loader_start_time.is_null()) {
    LogMilestoneHistogram(NavigationMilestone::kLoaderStart,
                          latest_navigation_handle_timing_.loader_start_time,
                          navigation_start_time_);
  }

  last_logged_navigation_handle_timing_ = latest_navigation_handle_timing_;
}

void AbandonedPageLoadMetricsObserver::LogNetError(
    net::Error error_code,
    base::TimeTicks navigation_abandon_time) {
  // `milestone` is only used to get `base_suffix`. Actually NetError
  // abandonments mostly happen in the LoaderStart milestone. So we don't log
  // the milestone explicitly not to explode the histogram combination.
  NavigationMilestone milestone =
      !latest_navigation_handle_timing_.loader_start_time.is_null() &&
              navigation_abandon_time >
                  latest_navigation_handle_timing_.loader_start_time
          ? NavigationMilestone::kLoaderStart
          : NavigationMilestone::kNavigationStart;
  std::string base_suffix =
      GetHistogramSuffix(milestone, navigation_abandon_time);
  const std::string taken_time_suffix = internal::GetSuffixForAbandonedTime(
      navigation_abandon_time - navigation_start_time_);
  for (std::string additional_suffix : GetAdditionalSuffixes()) {
    std::string suffix = base_suffix + additional_suffix;
    // Network error codes are negative. See: src/net/base/net_error_list.h.
    base::UmaHistogramSparse(
        base::StrCat({GetHistogramPrefix(), "NetError", suffix}),
        std::abs(error_code));
    base::UmaHistogramSparse(base::StrCat({GetHistogramPrefix(), "NetError",
                                           suffix, taken_time_suffix}),
                             std::abs(error_code));
  }
}
