// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_scheduler_host.h"

#include <string>
#include <utility>
#include <vector>

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/feed/core/pref_names.h"
#include "components/feed/core/time_serialization.h"
#include "components/feed/feed_feature_list.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/web_resource/web_resource_pref_names.h"
#include "net/base/network_change_notifier.h"

namespace feed {

namespace {

using TriggerType = FeedSchedulerHost::TriggerType;
using UserClass = UserClassifier::UserClass;

struct ParamPair {
  std::string name;
  double default_value;
};

const base::FeatureParam<int> kSuppressRefreshDurationMinutes{
    &kInterestFeedContentSuggestions, "suppress_refresh_duration_minutes", 30};
const base::FeatureParam<std::string> kDisableTriggerTypes{
    &kInterestFeedContentSuggestions, "disable_trigger_types", ""};

// The Cartesian product of TriggerType and UserClass each need a different
// param name in case we decide to change it via a config change. This nested
// switch lookup ensures that all combinations are defined, along with a
// default value.
ParamPair LookupParam(UserClass user_class, TriggerType trigger) {
  switch (user_class) {
    case UserClass::kRareSuggestionsViewer:
      switch (trigger) {
        case TriggerType::kNtpShown:
          return {"ntp_shown_hours_rare_ntp_user", 4.0};
        case TriggerType::kForegrounded:
          return {"foregrounded_hours_rare_ntp_user", 24.0};
        case TriggerType::kFixedTimer:
          return {"fixed_timer_hours_rare_ntp_user", 96.0};
      }
    case UserClass::kActiveSuggestionsViewer:
      switch (trigger) {
        case TriggerType::kNtpShown:
          return {"ntp_shown_hours_active_ntp_user", 4.0};
        case TriggerType::kForegrounded:
          return {"foregrounded_hours_active_ntp_user", 24.0};
        case TriggerType::kFixedTimer:
          return {"fixed_timer_hours_active_ntp_user", 48.0};
      }
    case UserClass::kActiveSuggestionsConsumer:
      switch (trigger) {
        case TriggerType::kNtpShown:
          return {"ntp_shown_hours_active_suggestions_consumer", 1.0};
        case TriggerType::kForegrounded:
          return {"foregrounded_hours_active_suggestions_consumer", 12.0};
        case TriggerType::kFixedTimer:
          return {"fixed_timer_hours_active_suggestions_consumer", 24.0};
      }
  }
}

// Coverts from base::StringPiece to TriggerType and adds it to the set if the
// trigger is recognized. Otherwise it is ignored.
void TryAddTriggerType(base::StringPiece trigger_as_string_piece,
                       std::set<TriggerType>* trigger_set) {
  static_assert(static_cast<unsigned int>(TriggerType::kMaxValue) == 2,
                "New TriggerTypes must be handled below.");
  if (trigger_as_string_piece == "ntp_shown") {
    trigger_set->insert(TriggerType::kNtpShown);
  } else if (trigger_as_string_piece == "foregrounded") {
    trigger_set->insert(TriggerType::kForegrounded);
  } else if (trigger_as_string_piece == "fixed_timer") {
    trigger_set->insert(TriggerType::kFixedTimer);
  }
}

// Generates a set of disabled triggers.
std::set<TriggerType> GetDisabledTriggerTypes() {
  std::set<TriggerType> disabled_triggers;

  // Do not in-line FeatureParam::Get(), |param_value| must stay alive while
  // StringPieces reference segments.
  std::string param_value = kDisableTriggerTypes.Get();

  for (auto token :
       base::SplitStringPiece(param_value, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    TryAddTriggerType(token, &disabled_triggers);
  }
  return disabled_triggers;
}

// Run the given closure if it is valid.
void TryRun(base::OnceClosure closure) {
  if (closure) {
    std::move(closure).Run();
  }
}

// Converts UserClassifier::UserClass to a string that corresponds to the
// entries in histogram suffix "UserClasses".
std::string UserClassToHistogramSuffix(UserClassifier::UserClass user_class) {
  switch (user_class) {
    case UserClassifier::UserClass::kRareSuggestionsViewer:
      return "RareNTPUser";
    case UserClassifier::UserClass::kActiveSuggestionsViewer:
      return "ActiveNTPUser";
    case UserClassifier::UserClass::kActiveSuggestionsConsumer:
      return "ActiveSuggestionsConsumer";
  }
}

// This has a small performance penalty because it is looking up the histogram
// dynamically, which avoids a significantly amount of boilerplate code for the
// various |qualified_trigger| and user class strings. This is reasonable
// because this method is only called as a result of a direct user interaction,
// like opening the NTP or foregrounding the browser.
void ReportAgeWithSuffix(const std::string& qualified_trigger,
                         UserClassifier::UserClass user_class,
                         base::TimeDelta sample) {
  std::string name = base::StringPrintf(
      "NewTabPage.ContentSuggestions.%s.%s", qualified_trigger.c_str(),
      UserClassToHistogramSuffix(user_class).c_str());
  base::UmaHistogramCustomTimes(name, sample, base::TimeDelta::FromSeconds(1),
                                base::TimeDelta::FromDays(7),
                                /*bucket_count=*/50);
}

}  // namespace

FeedSchedulerHost::FeedSchedulerHost(PrefService* profile_prefs,
                                     PrefService* local_state,
                                     base::Clock* clock)
    : profile_prefs_(profile_prefs),
      clock_(clock),
      user_classifier_(profile_prefs, clock),
      disabled_triggers_(GetDisabledTriggerTypes()),
      eula_accepted_notifier_(
          web_resource::EulaAcceptedNotifier::Create(local_state)) {
  if (eula_accepted_notifier_) {
    eula_accepted_notifier_->Init(this);
  }

  throttlers_.emplace(UserClassifier::UserClass::kRareSuggestionsViewer,
                      std::make_unique<RefreshThrottler>(
                          UserClassifier::UserClass::kRareSuggestionsViewer,
                          profile_prefs_, clock_));
  throttlers_.emplace(UserClassifier::UserClass::kActiveSuggestionsViewer,
                      std::make_unique<RefreshThrottler>(
                          UserClassifier::UserClass::kActiveSuggestionsViewer,
                          profile_prefs_, clock_));
  throttlers_.emplace(UserClassifier::UserClass::kActiveSuggestionsConsumer,
                      std::make_unique<RefreshThrottler>(
                          UserClassifier::UserClass::kActiveSuggestionsConsumer,
                          profile_prefs_, clock_));
}

FeedSchedulerHost::~FeedSchedulerHost() = default;

// static
void FeedSchedulerHost::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimePref(prefs::kLastFetchAttemptTime, base::Time());
  registry->RegisterTimeDeltaPref(prefs::kBackgroundRefreshPeriod,
                                  base::TimeDelta());
}

void FeedSchedulerHost::Initialize(
    base::RepeatingClosure refresh_callback,
    ScheduleBackgroundTaskCallback schedule_background_task_callback) {
  // There should only ever be one scheduler host and bridge created. Neither
  // are ever destroyed before shutdown, and this method should only be called
  // once as the bridge is constructed.
  DCHECK(!refresh_callback_);
  DCHECK(!schedule_background_task_callback_);

  refresh_callback_ = std::move(refresh_callback);
  schedule_background_task_callback_ =
      std::move(schedule_background_task_callback);

  base::TimeDelta old_period =
      profile_prefs_->GetTimeDelta(prefs::kBackgroundRefreshPeriod);
  base::TimeDelta new_period = GetTriggerThreshold(TriggerType::kFixedTimer);
  if (old_period != new_period) {
    ScheduleFixedTimerWakeUp(new_period);
  }
}

NativeRequestBehavior FeedSchedulerHost::ShouldSessionRequestData(
    bool has_content,
    base::Time content_creation_date_time,
    bool has_outstanding_request) {
  // The scheduler may not always know of outstanding requests, but the Feed
  // should know about them all, and the scheduler should be notified upon
  // completion of all requests. We should never encounter a scenario where only
  // the scheduler thinks there is an outstanding request.

  // TODO(skym): Resolve ambiguity around this expectation.
  // DCHECK(has_outstanding_request || !tracking_oustanding_request_);

  tracking_oustanding_request_ |= has_outstanding_request;

  NativeRequestBehavior behavior;
  if (ShouldRefresh(TriggerType::kNtpShown)) {
    if (!has_content) {
      behavior = kRequestWithWait;
    } else if (IsContentStale(content_creation_date_time)) {
      behavior = kRequestWithTimeout;
    } else {
      behavior = kRequestWithContent;
    }
  } else {
    // Note that kNoRequestWithWait is used to show a blank article section
    // even when no request is being made. The user will be given the ability to
    // force a refresh but this scheduler is not driving it.
    if (!has_content) {
      behavior = kNoRequestWithWait;
    } else if (IsContentStale(content_creation_date_time) &&
               has_outstanding_request) {
      // This needs to check |has_outstanding_request|, it does not make sense
      // to use a timeout when no request is being made. Just show the stale
      // content, since nothing better is on the way.
      behavior = kNoRequestWithTimeout;
    } else {
      behavior = kNoRequestWithContent;
    }
  }

  OnSuggestionsShown();
  DVLOG(2) << "Specifying NativeRequestBehavior of "
           << static_cast<int>(behavior);
  UMA_HISTOGRAM_ENUMERATION("ContentSuggestions.Feed.Scheduler.RequestBehavior",
                            behavior);
  return behavior;
}

void FeedSchedulerHost::OnReceiveNewContent(
    base::Time content_creation_date_time) {
  profile_prefs_->SetTime(prefs::kLastFetchAttemptTime,
                          content_creation_date_time);
  TryRun(std::move(fixed_timer_completion_));
  ScheduleFixedTimerWakeUp(GetTriggerThreshold(TriggerType::kFixedTimer));
  tracking_oustanding_request_ = false;
  time_until_first_shown_trigger_reported_ = false;
  time_until_first_foregrounded_trigger_reported_ = false;
  DVLOG(2) << "Received OnReceiveNewContent with time "
           << content_creation_date_time;
}

void FeedSchedulerHost::OnRequestError(int network_response_code) {
  profile_prefs_->SetTime(prefs::kLastFetchAttemptTime, clock_->Now());
  TryRun(std::move(fixed_timer_completion_));
  tracking_oustanding_request_ = false;
  time_until_first_shown_trigger_reported_ = false;
  time_until_first_foregrounded_trigger_reported_ = false;
  DVLOG(2) << "Received OnRequestError with code " << network_response_code;
}

void FeedSchedulerHost::OnForegrounded() {
  DCHECK(refresh_callback_);
  if (ShouldRefresh(TriggerType::kForegrounded)) {
    refresh_callback_.Run();
  }
}

void FeedSchedulerHost::OnFixedTimer(base::OnceClosure on_completion) {
  DCHECK(refresh_callback_);
  if (ShouldRefresh(TriggerType::kFixedTimer)) {
    // There shouldn't typically be anything in |fixed_timer_completion_| right
    // now, but if there was, run it before we replace it.
    TryRun(std::move(fixed_timer_completion_));

    fixed_timer_completion_ = std::move(on_completion);
    refresh_callback_.Run();
  } else {
    // The task driving this doesn't need to stay around, since no work is being
    // done on its behalf.
    TryRun(std::move(on_completion));
  }
}

void FeedSchedulerHost::OnTaskReschedule() {
  ScheduleFixedTimerWakeUp(GetTriggerThreshold(TriggerType::kFixedTimer));
}

void FeedSchedulerHost::OnSuggestionConsumed() {
  user_classifier_.OnEvent(UserClassifier::Event::kSuggestionsUsed);
}

void FeedSchedulerHost::OnSuggestionsShown() {
  user_classifier_.OnEvent(UserClassifier::Event::kSuggestionsViewed);
}

void FeedSchedulerHost::OnHistoryCleared() {
  // Due to privacy, we should not fetch for a while (unless the user explicitly
  // asks for new suggestions) to give sync the time to propagate the changes in
  // history to the server.
  suppress_refreshes_until_ =
      clock_->Now() +
      base::TimeDelta::FromMinutes(kSuppressRefreshDurationMinutes.Get());
  // After that time elapses, we should fetch as soon as possible.
  profile_prefs_->ClearPref(prefs::kLastFetchAttemptTime);
}

void FeedSchedulerHost::OnEulaAccepted() {
  OnForegrounded();
}

bool FeedSchedulerHost::ShouldRefresh(TriggerType trigger) {
  if (tracking_oustanding_request_) {
    DVLOG(2) << "Outstanding request stopped refresh from trigger "
             << static_cast<int>(trigger);
    return false;
  }

  if (base::ContainsKey(disabled_triggers_, trigger)) {
    DVLOG(2) << "Disabled trigger stopped refresh from trigger "
             << static_cast<int>(trigger);
    return false;
  }

  if (net::NetworkChangeNotifier::IsOffline()) {
    DVLOG(2) << "Network is offline stopped refresh from trigger "
             << static_cast<int>(trigger);
    return false;
  }

  if (eula_accepted_notifier_ && !eula_accepted_notifier_->IsEulaAccepted()) {
    DVLOG(2) << "EULA not being accepted stopped refresh from trigger "
             << static_cast<int>(trigger);
    return false;
  }

  base::TimeDelta attempt_age =
      clock_->Now() - profile_prefs_->GetTime(prefs::kLastFetchAttemptTime);
  UserClassifier::UserClass user_class = user_classifier_.GetUserClass();

  if (trigger == TriggerType::kNtpShown &&
      !time_until_first_shown_trigger_reported_) {
    time_until_first_shown_trigger_reported_ = true;
    ReportAgeWithSuffix("TimeUntilFirstShownTrigger", user_class, attempt_age);
  }

  if (trigger == TriggerType::kForegrounded &&
      !time_until_first_foregrounded_trigger_reported_) {
    time_until_first_foregrounded_trigger_reported_ = true;
    ReportAgeWithSuffix("TimeUntilFirstStartupTrigger", user_class,
                        attempt_age);
  }

  if (clock_->Now() < suppress_refreshes_until_) {
    DVLOG(2) << "Refresh suppression until " << suppress_refreshes_until_
             << " stopped refresh from trigger " << static_cast<int>(trigger);
    return false;
  }

  if (attempt_age < GetTriggerThreshold(trigger)) {
    DVLOG(2) << "Last attempt age of " << attempt_age
             << " stopped refresh from trigger " << static_cast<int>(trigger);
    return false;
  }

  auto throttlerIter = throttlers_.find(user_class);
  if (throttlerIter == throttlers_.end() ||
      !throttlerIter->second->RequestQuota()) {
    DVLOG(2) << "Throttler stopped refresh from trigger "
             << static_cast<int>(trigger);
    return false;
  }

  switch (trigger) {
    case TriggerType::kNtpShown:
      ReportAgeWithSuffix("TimeUntilSoftFetch", user_class, attempt_age);
      break;
    case TriggerType::kForegrounded:
      ReportAgeWithSuffix("TimeUntilStartupFetch", user_class, attempt_age);
      break;
    case TriggerType::kFixedTimer:
      ReportAgeWithSuffix("TimeUntilPersistentFetch", user_class, attempt_age);
      break;
  }

  DVLOG(2) << "Requesting refresh from trigger " << static_cast<int>(trigger);
  UMA_HISTOGRAM_ENUMERATION("ContentSuggestions.Feed.Scheduler.RefreshTrigger",
                            trigger);
  tracking_oustanding_request_ = true;
  return true;
}

bool FeedSchedulerHost::IsContentStale(base::Time content_creation_date_time) {
  return (clock_->Now() - content_creation_date_time) >
         GetTriggerThreshold(TriggerType::kForegrounded);
}

base::TimeDelta FeedSchedulerHost::GetTriggerThreshold(TriggerType trigger) {
  UserClass user_class = user_classifier_.GetUserClass();
  ParamPair param = LookupParam(user_class, trigger);
  double value_hours = base::GetFieldTrialParamByFeatureAsDouble(
      kInterestFeedContentSuggestions, param.name, param.default_value);

  // Use FromSecondsD in case one of the values contained a decimal.
  return base::TimeDelta::FromSecondsD(value_hours * 3600.0);
}

void FeedSchedulerHost::ScheduleFixedTimerWakeUp(base::TimeDelta period) {
  profile_prefs_->SetTimeDelta(prefs::kBackgroundRefreshPeriod, period);
  schedule_background_task_callback_.Run(period);
}

}  // namespace feed
