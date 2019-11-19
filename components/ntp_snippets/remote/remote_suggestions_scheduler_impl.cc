// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/remote_suggestions_scheduler_impl.h"

#include <cfloat>
#include <random>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/time/clock.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/remote/persistent_scheduler.h"
#include "components/ntp_snippets/remote/remote_suggestions_provider.h"
#include "components/ntp_snippets/status.h"
#include "components/ntp_snippets/user_classifier.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "net/base/network_change_notifier.h"

namespace ntp_snippets {

namespace {

// The FetchingInterval enum specifies overlapping time intervals that are used
// for scheduling the next remote suggestion fetch. Therefore a timer is created
// for each interval. Initially all the timers are started at the same time.
// A fetch for a given interval is only performed at the moment (after the
// interval has elapsed) when the combination of two conditions associated with
// the interval is met.
// 1. Trigger contidion:
//   - STARTUP_*: the user starts / switches to Chrome;
//   - SHOWN_*: the suggestions surface is shown to the user;
//   - PERSISTENT_*: the OS initiates the scheduled fetching task (which has
//   been scheduled according to this interval).
// 2. Connectivity condition:
//   - *_WIFI: the user is on a connection that the OS classifies as unmetered;
//   - *_FALLBACK: holds for any functional internet connection.
//
// If a fetch failed, then only the corresponding timer is reset. The other
// timers are not touched.
enum class FetchingInterval {
  PERSISTENT_FALLBACK,
  PERSISTENT_WIFI,
  STARTUP_FALLBACK,
  STARTUP_WIFI,
  SHOWN_FALLBACK,
  SHOWN_WIFI,
  COUNT
};

// The following arrays specify default values for remote suggestions fetch
// intervals corresponding to individual user classes. The user classes are
// defined by the user classifier. There must be an array for each user class.
// The values of each array specify a default time interval for the intervals
// defined by the enum FetchingInterval. The default time intervals defined in
// the arrays can be overridden using different variation parameters.
const double kDefaultFetchingIntervalHoursRareNtpUser[] = {96.0, 96.0, 24.0,
                                                           24.0, 4.0,  4.0};
const double kDefaultFetchingIntervalHoursActiveNtpUser[] = {48.0, 48.0, 24.0,
                                                             24.0, 4.0,  4.0};
const double kDefaultFetchingIntervalHoursActiveSuggestionsConsumer[] = {
    24.0, 24.0, 12.0, 12.0, 1.0, 1.0};

// For a simple comparison: fetching intervals that emulate the state really
// rolled out to 100% M58 Stable. Used for evaluation of later changes. DBL_MAX
// values simulate this interval being disabled.
// TODO(jkrcal): Remove when not needed any more, incl. the feature. Probably
// after M62 when CH is launched.
const double kM58FetchingIntervalHoursRareNtpUser[] = {48.0,    24.0, DBL_MAX,
                                                       DBL_MAX, 4.0,  4.0};
const double kM58FetchingIntervalHoursActiveNtpUser[] = {24.0,    8.0,  DBL_MAX,
                                                         DBL_MAX, 10.0, 10.0};
const double kM58FetchingIntervalHoursActiveSuggestionsConsumer[] = {
    24.0, 6.0, DBL_MAX, DBL_MAX, 1.0, 1.0};

// Variation parameters than can be used to override the default fetching
// intervals. For backwards compatibility, we do not rename
//  - the "fetching_" params (should be "persistent_fetching_");
//  - the "soft_" params (should be "shown_").
const char* kFetchingIntervalParamNameRareNtpUser[] = {
    "fetching_interval_hours-fallback-rare_ntp_user",
    "fetching_interval_hours-wifi-rare_ntp_user",
    "startup_fetching_interval_hours-fallback-rare_ntp_user",
    "startup_fetching_interval_hours-wifi-rare_ntp_user",
    "soft_fetching_interval_hours-fallback-rare_ntp_user",
    "soft_fetching_interval_hours-wifi-rare_ntp_user"};
const char* kFetchingIntervalParamNameActiveNtpUser[] = {
    "fetching_interval_hours-fallback-active_ntp_user",
    "fetching_interval_hours-wifi-active_ntp_user",
    "startup_fetching_interval_hours-fallback-active_ntp_user",
    "startup_fetching_interval_hours-wifi-active_ntp_user",
    "soft_fetching_interval_hours-fallback-active_ntp_user",
    "soft_fetching_interval_hours-wifi-active_ntp_user"};
const char* kFetchingIntervalParamNameActiveSuggestionsConsumer[] = {
    "fetching_interval_hours-fallback-active_suggestions_consumer",
    "fetching_interval_hours-wifi-active_suggestions_consumer",
    "startup_fetching_interval_hours-fallback-active_suggestions_consumer",
    "startup_fetching_interval_hours-wifi-active_suggestions_consumer",
    "soft_fetching_interval_hours-fallback-active_suggestions_consumer",
    "soft_fetching_interval_hours-wifi-active_suggestions_consumer"};

static_assert(
    static_cast<unsigned int>(FetchingInterval::COUNT) ==
            base::size(kDefaultFetchingIntervalHoursRareNtpUser) &&
        static_cast<unsigned int>(FetchingInterval::COUNT) ==
            base::size(kDefaultFetchingIntervalHoursActiveNtpUser) &&
        static_cast<unsigned int>(FetchingInterval::COUNT) ==
            base::size(
                kDefaultFetchingIntervalHoursActiveSuggestionsConsumer) &&
        static_cast<unsigned int>(FetchingInterval::COUNT) ==
            base::size(kM58FetchingIntervalHoursRareNtpUser) &&
        static_cast<unsigned int>(FetchingInterval::COUNT) ==
            base::size(kM58FetchingIntervalHoursActiveNtpUser) &&
        static_cast<unsigned int>(FetchingInterval::COUNT) ==
            base::size(kM58FetchingIntervalHoursActiveSuggestionsConsumer) &&
        static_cast<unsigned int>(FetchingInterval::COUNT) ==
            base::size(kFetchingIntervalParamNameRareNtpUser) &&
        static_cast<unsigned int>(FetchingInterval::COUNT) ==
            base::size(kFetchingIntervalParamNameActiveNtpUser) &&
        static_cast<unsigned int>(FetchingInterval::COUNT) ==
            base::size(kFetchingIntervalParamNameActiveSuggestionsConsumer),
    "Fill in all the info for fetching intervals.");

// For backward compatibility "ntp_opened" value is kept and denotes the
// SURFACE_OPENED trigger type.
const char* const kTriggerTypeNames[] = {"persistent_scheduler_wake_up",
                                         "ntp_opened", "browser_foregrounded",
                                         "browser_cold_start"};

const char* const kTriggerTypesParamName = "scheduler_trigger_types";
const char* const kTriggerTypesParamValueForEmptyList = "-";

const int kBlockBackgroundFetchesMinutesAfterClearingHistory = 30;

// Variation parameter for minimal age of a fetch to be considered "stale".
const char kMinAgeForStaleFetchHoursParamName[] =
    "min_age_for_stale_fetch_hours";

// Returns the time interval to use for scheduling remote suggestion fetches for
// the given interval and user_class.
base::TimeDelta GetDesiredFetchingInterval(
    FetchingInterval interval,
    UserClassifier::UserClass user_class) {
  DCHECK(interval != FetchingInterval::COUNT);
  const unsigned int index = static_cast<unsigned int>(interval);
  DCHECK(index < base::size(kDefaultFetchingIntervalHoursRareNtpUser));

  bool emulateM58 = base::FeatureList::IsEnabled(
      kRemoteSuggestionsEmulateM58FetchingSchedule);

  double default_value_hours = 0.0;
  const char* param_name = nullptr;
  switch (user_class) {
    case UserClassifier::UserClass::RARE_NTP_USER:
      default_value_hours =
          emulateM58 ? kM58FetchingIntervalHoursRareNtpUser[index]
                     : kDefaultFetchingIntervalHoursRareNtpUser[index];
      param_name = kFetchingIntervalParamNameRareNtpUser[index];
      break;
    case UserClassifier::UserClass::ACTIVE_NTP_USER:
      default_value_hours =
          emulateM58 ? kM58FetchingIntervalHoursActiveNtpUser[index]
                     : kDefaultFetchingIntervalHoursActiveNtpUser[index];
      param_name = kFetchingIntervalParamNameActiveNtpUser[index];
      break;
    case UserClassifier::UserClass::ACTIVE_SUGGESTIONS_CONSUMER:
      default_value_hours =
          emulateM58
              ? kM58FetchingIntervalHoursActiveSuggestionsConsumer[index]
              : kDefaultFetchingIntervalHoursActiveSuggestionsConsumer[index];
      param_name = kFetchingIntervalParamNameActiveSuggestionsConsumer[index];
      break;
  }

  double value_hours = base::GetFieldTrialParamByFeatureAsDouble(
      ntp_snippets::kArticleSuggestionsFeature, param_name,
      default_value_hours);

  return base::TimeDelta::FromSecondsD(value_hours * 3600.0);
}

void ReportTimeUntilFirstShownTrigger(
    UserClassifier::UserClass user_class,
    base::TimeDelta time_until_first_shown_trigger) {
  switch (user_class) {
    case UserClassifier::UserClass::RARE_NTP_USER:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "NewTabPage.ContentSuggestions.TimeUntilFirstShownTrigger."
          "RareNTPUser",
          time_until_first_shown_trigger, base::TimeDelta::FromSeconds(1),
          base::TimeDelta::FromDays(7),
          /*bucket_count=*/50);
      break;
    case UserClassifier::UserClass::ACTIVE_NTP_USER:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "NewTabPage.ContentSuggestions.TimeUntilFirstShownTrigger."
          "ActiveNTPUser",
          time_until_first_shown_trigger, base::TimeDelta::FromSeconds(1),
          base::TimeDelta::FromDays(7),
          /*bucket_count=*/50);
      break;
    case UserClassifier::UserClass::ACTIVE_SUGGESTIONS_CONSUMER:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "NewTabPage.ContentSuggestions.TimeUntilFirstShownTrigger."
          "ActiveSuggestionsConsumer",
          time_until_first_shown_trigger, base::TimeDelta::FromSeconds(1),
          base::TimeDelta::FromDays(7),
          /*bucket_count=*/50);
      break;
  }
}

void ReportTimeUntilFirstStartupTrigger(
    UserClassifier::UserClass user_class,
    base::TimeDelta time_until_first_startup_trigger) {
  switch (user_class) {
    case UserClassifier::UserClass::RARE_NTP_USER:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "NewTabPage.ContentSuggestions.TimeUntilFirstStartupTrigger."
          "RareNTPUser",
          time_until_first_startup_trigger, base::TimeDelta::FromSeconds(1),
          base::TimeDelta::FromDays(7),
          /*bucket_count=*/50);
      break;
    case UserClassifier::UserClass::ACTIVE_NTP_USER:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "NewTabPage.ContentSuggestions.TimeUntilFirstStartupTrigger."
          "ActiveNTPUser",
          time_until_first_startup_trigger, base::TimeDelta::FromSeconds(1),
          base::TimeDelta::FromDays(7),
          /*bucket_count=*/50);
      break;
    case UserClassifier::UserClass::ACTIVE_SUGGESTIONS_CONSUMER:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "NewTabPage.ContentSuggestions.TimeUntilFirstStartupTrigger."
          "ActiveSuggestionsConsumer",
          time_until_first_startup_trigger, base::TimeDelta::FromSeconds(1),
          base::TimeDelta::FromDays(7),
          /*bucket_count=*/50);
      break;
  }
}

void ReportTimeUntilShownFetch(UserClassifier::UserClass user_class,
                               base::TimeDelta time_until_shown_fetch) {
  switch (user_class) {
    case UserClassifier::UserClass::RARE_NTP_USER:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "NewTabPage.ContentSuggestions.TimeUntilSoftFetch."
          "RareNTPUser",
          time_until_shown_fetch, base::TimeDelta::FromSeconds(1),
          base::TimeDelta::FromDays(7),
          /*bucket_count=*/50);
      break;
    case UserClassifier::UserClass::ACTIVE_NTP_USER:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "NewTabPage.ContentSuggestions.TimeUntilSoftFetch."
          "ActiveNTPUser",
          time_until_shown_fetch, base::TimeDelta::FromSeconds(1),
          base::TimeDelta::FromDays(7),
          /*bucket_count=*/50);
      break;
    case UserClassifier::UserClass::ACTIVE_SUGGESTIONS_CONSUMER:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "NewTabPage.ContentSuggestions.TimeUntilSoftFetch."
          "ActiveSuggestionsConsumer",
          time_until_shown_fetch, base::TimeDelta::FromSeconds(1),
          base::TimeDelta::FromDays(7),
          /*bucket_count=*/50);
      break;
  }
}

void ReportTimeUntilStartupFetch(UserClassifier::UserClass user_class,
                                 base::TimeDelta time_until_startup_fetch) {
  switch (user_class) {
    case UserClassifier::UserClass::RARE_NTP_USER:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "NewTabPage.ContentSuggestions.TimeUntilStartupFetch."
          "RareNTPUser",
          time_until_startup_fetch, base::TimeDelta::FromSeconds(1),
          base::TimeDelta::FromDays(7),
          /*bucket_count=*/50);
      break;
    case UserClassifier::UserClass::ACTIVE_NTP_USER:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "NewTabPage.ContentSuggestions.TimeUntilStartupFetch."
          "ActiveNTPUser",
          time_until_startup_fetch, base::TimeDelta::FromSeconds(1),
          base::TimeDelta::FromDays(7),
          /*bucket_count=*/50);
      break;
    case UserClassifier::UserClass::ACTIVE_SUGGESTIONS_CONSUMER:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "NewTabPage.ContentSuggestions.TimeUntilStartupFetch."
          "ActiveSuggestionsConsumer",
          time_until_startup_fetch, base::TimeDelta::FromSeconds(1),
          base::TimeDelta::FromDays(7),
          /*bucket_count=*/50);
      break;
  }
}

void ReportTimeUntilPersistentFetch(
    UserClassifier::UserClass user_class,
    base::TimeDelta time_until_persistent_fetch) {
  switch (user_class) {
    case UserClassifier::UserClass::RARE_NTP_USER:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "NewTabPage.ContentSuggestions.TimeUntilPersistentFetch."
          "RareNTPUser",
          time_until_persistent_fetch, base::TimeDelta::FromSeconds(1),
          base::TimeDelta::FromDays(7),
          /*bucket_count=*/50);
      break;
    case UserClassifier::UserClass::ACTIVE_NTP_USER:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "NewTabPage.ContentSuggestions.TimeUntilPersistentFetch."
          "ActiveNTPUser",
          time_until_persistent_fetch, base::TimeDelta::FromSeconds(1),
          base::TimeDelta::FromDays(7),
          /*bucket_count=*/50);
      break;
    case UserClassifier::UserClass::ACTIVE_SUGGESTIONS_CONSUMER:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "NewTabPage.ContentSuggestions.TimeUntilPersistentFetch."
          "ActiveSuggestionsConsumer",
          time_until_persistent_fetch, base::TimeDelta::FromSeconds(1),
          base::TimeDelta::FromDays(7),
          /*bucket_count=*/50);
      break;
  }
}

}  // namespace

class EulaState final : public web_resource::EulaAcceptedNotifier::Observer {
 public:
  EulaState(PrefService* local_state_prefs, base::Closure eula_accepted)
      : eula_notifier_(
            web_resource::EulaAcceptedNotifier::Create(local_state_prefs)),
        eula_accepted_(eula_accepted) {
    // EulaNotifier is not constructed on some platforms (such as desktop).
    if (!eula_notifier_) {
      return;
    }

    // Register the observer.
    eula_notifier_->Init(this);
  }

  ~EulaState() override = default;

  bool IsEulaAccepted() {
    if (!eula_notifier_) {
      return true;
    }
    return eula_notifier_->IsEulaAccepted();
  }

  // EulaAcceptedNotifier::Observer implementation.
  void OnEulaAccepted() override {
    // Note that this code is only run if a previous call to IsEulaAccepted()
    // returned false. In that case, the prefs are watched and this method gets
    // executed once the setting flips to accepted. Hence, we can assume that
    // at the time this code runs, a background-fetch trigger is queued in the
    // scheduler.
    eula_accepted_.Run();
  }

 private:
  std::unique_ptr<web_resource::EulaAcceptedNotifier> eula_notifier_;
  base::Callback<void()> eula_accepted_;

  DISALLOW_COPY_AND_ASSIGN(EulaState);
};

// static
RemoteSuggestionsSchedulerImpl::FetchingSchedule
RemoteSuggestionsSchedulerImpl::FetchingSchedule::Empty() {
  return FetchingSchedule{base::TimeDelta(), base::TimeDelta(),
                          base::TimeDelta(), base::TimeDelta()};
}

bool RemoteSuggestionsSchedulerImpl::FetchingSchedule::operator==(
    const FetchingSchedule& other) const {
  return interval_persistent_wifi == other.interval_persistent_wifi &&
         interval_persistent_fallback == other.interval_persistent_fallback &&
         interval_startup_wifi == other.interval_startup_wifi &&
         interval_startup_fallback == other.interval_startup_fallback &&
         interval_shown_wifi == other.interval_shown_wifi &&
         interval_shown_fallback == other.interval_shown_fallback;
}

bool RemoteSuggestionsSchedulerImpl::FetchingSchedule::operator!=(
    const FetchingSchedule& other) const {
  return !operator==(other);
}

bool RemoteSuggestionsSchedulerImpl::FetchingSchedule::is_empty() const {
  return interval_persistent_wifi.is_zero() &&
         interval_persistent_fallback.is_zero() &&
         interval_startup_wifi.is_zero() &&
         interval_startup_fallback.is_zero() && interval_shown_wifi.is_zero() &&
         interval_shown_fallback.is_zero();
}

base::TimeDelta
RemoteSuggestionsSchedulerImpl::FetchingSchedule::GetStalenessInterval() const {
  // The default value for staleness is |interval_startup_wifi| which is not
  // constant. It depends on user class and is configurable by field trial
  // params as well.
  return base::TimeDelta::FromHours(base::GetFieldTrialParamByFeatureAsInt(
      ntp_snippets::kArticleSuggestionsFeature,
      kMinAgeForStaleFetchHoursParamName, interval_startup_wifi.InHours()));
}

// The TriggerType enum specifies values for the events that can trigger
// fetching remote suggestions. These values are written to logs. New enum
// values can be added, but existing enums must never be renumbered or deleted
// and reused. When adding new entries, also update the array
// |kTriggerTypeNames| above.
enum class RemoteSuggestionsSchedulerImpl::TriggerType {
  PERSISTENT_SCHEDULER_WAKE_UP = 0,
  SURFACE_OPENED = 1,
  BROWSER_FOREGROUNDED = 2,
  BROWSER_COLD_START = 3,
  COUNT
};

RemoteSuggestionsSchedulerImpl::RemoteSuggestionsSchedulerImpl(
    PersistentScheduler* persistent_scheduler,
    const UserClassifier* user_classifier,
    PrefService* profile_prefs,
    PrefService* local_state_prefs,
    base::Clock* clock)
    : persistent_scheduler_(persistent_scheduler),
      provider_(nullptr),
      background_fetch_in_progress_(false),
      user_classifier_(user_classifier),
      request_throttler_rare_ntp_user_(
          profile_prefs,
          RequestThrottler::RequestType::
              CONTENT_SUGGESTION_FETCHER_RARE_NTP_USER),
      request_throttler_active_ntp_user_(
          profile_prefs,
          RequestThrottler::RequestType::
              CONTENT_SUGGESTION_FETCHER_ACTIVE_NTP_USER),
      request_throttler_active_suggestions_consumer_(
          profile_prefs,
          RequestThrottler::RequestType::
              CONTENT_SUGGESTION_FETCHER_ACTIVE_SUGGESTIONS_CONSUMER),
      time_until_first_shown_trigger_reported_(false),
      time_until_first_startup_trigger_reported_(false),
      eula_state_(std::make_unique<EulaState>(
          local_state_prefs,
          base::Bind(&RemoteSuggestionsSchedulerImpl::RunQueuedTriggersIfReady,
                     base::Unretained(this)))),
      profile_prefs_(profile_prefs),
      clock_(clock),
      enabled_triggers_(GetEnabledTriggerTypes()) {
  DCHECK(user_classifier);
  DCHECK(profile_prefs);
}

RemoteSuggestionsSchedulerImpl::~RemoteSuggestionsSchedulerImpl() = default;

// static
void RemoteSuggestionsSchedulerImpl::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterTimeDeltaPref(prefs::kSnippetPersistentFetchingIntervalWifi,
                                  base::TimeDelta());
  registry->RegisterTimeDeltaPref(
      prefs::kSnippetPersistentFetchingIntervalFallback, base::TimeDelta());
  registry->RegisterTimeDeltaPref(prefs::kSnippetStartupFetchingIntervalWifi,
                                  base::TimeDelta());
  registry->RegisterTimeDeltaPref(
      prefs::kSnippetStartupFetchingIntervalFallback, base::TimeDelta());
  registry->RegisterTimeDeltaPref(prefs::kSnippetShownFetchingIntervalWifi,
                                  base::TimeDelta());
  registry->RegisterTimeDeltaPref(prefs::kSnippetShownFetchingIntervalFallback,
                                  base::TimeDelta());
  registry->RegisterTimePref(prefs::kSnippetLastFetchAttemptTime, base::Time());
  registry->RegisterTimePref(prefs::kSnippetLastSuccessfulFetchTime,
                             base::Time());
}

void RemoteSuggestionsSchedulerImpl::SetProvider(
    RemoteSuggestionsProvider* provider) {
  DCHECK(provider);
  provider_ = provider;
}

void RemoteSuggestionsSchedulerImpl::OnProviderActivated() {
  LoadLastFetchingSchedule();
  StartScheduling();
  RunQueuedTriggersIfReady();
}

void RemoteSuggestionsSchedulerImpl::RunQueuedTriggersIfReady() {
  // Process any queued triggers if we're now ready for background fetches.
  if (IsReadyForBackgroundFetches()) {
    std::set<TriggerType> queued_triggers_copy;
    queued_triggers_copy.swap(queued_triggers_);
    for (const TriggerType trigger : queued_triggers_copy) {
      RefetchIfAppropriate(trigger);
    }
  }
}

void RemoteSuggestionsSchedulerImpl::OnProviderDeactivated() {
  StopScheduling();
}

void RemoteSuggestionsSchedulerImpl::OnSuggestionsCleared() {
  // This should be called by |provider_| so it should exist.
  DCHECK(provider_);
  // Some user action causes suggestions to be cleared, we need to fetch as soon
  // as possible.
  ClearLastFetchAttemptTime();
  // We cannot guarantee that the surface is not visible when the event happens.
  // To make sure the suggestions get replaced, we use the SURFACE_OPENED
  // trigger.
  RefetchIfAppropriate(TriggerType::SURFACE_OPENED);
}

void RemoteSuggestionsSchedulerImpl::OnHistoryCleared() {
  // Due to privacy, we should not fetch for a while (unless the user explicitly
  // asks for new suggestions) to give sync the time to propagate the changes in
  // history to the server.
  background_fetches_allowed_after_ =
      clock_->Now() + base::TimeDelta::FromMinutes(
                          kBlockBackgroundFetchesMinutesAfterClearingHistory);
  // After that time elapses, we should fetch as soon as possible.
  ClearLastFetchAttemptTime();
}

void RemoteSuggestionsSchedulerImpl::OnBrowserUpgraded() {
  // After browser upgrade, persistent schedule needs to get reset. Force the
  // reschedule by stopping and starting it again.
  StopScheduling();
  StartScheduling();
}

bool RemoteSuggestionsSchedulerImpl::AcquireQuotaForInteractiveFetch() {
  return AcquireQuota(/*interactive_request=*/true);
}

void RemoteSuggestionsSchedulerImpl::OnInteractiveFetchFinished(
    Status fetch_status) {
  OnFetchCompleted(fetch_status);
}

void RemoteSuggestionsSchedulerImpl::OnPersistentSchedulerWakeUp() {
  RefetchIfAppropriate(TriggerType::PERSISTENT_SCHEDULER_WAKE_UP);
}

void RemoteSuggestionsSchedulerImpl::OnBrowserForegrounded() {
  // TODO(jkrcal): Consider that this is called whenever we open or return to an
  // Activity. Therefore, keep work light for fast start up calls.
  RefetchIfAppropriate(TriggerType::BROWSER_FOREGROUNDED);
}

void RemoteSuggestionsSchedulerImpl::OnBrowserColdStart() {
  // TODO(jkrcal): Consider that work here must be kept light for fast
  // cold start ups.
  RefetchIfAppropriate(TriggerType::BROWSER_COLD_START);
}

void RemoteSuggestionsSchedulerImpl::OnSuggestionsSurfaceOpened() {
  // TODO(jkrcal): Consider that this is called whenever we open an NTP.
  // Therefore, keep work light for fast start up calls.
  RefetchIfAppropriate(TriggerType::SURFACE_OPENED);
}

void RemoteSuggestionsSchedulerImpl::StartScheduling() {
  FetchingSchedule new_schedule = GetDesiredFetchingSchedule();

  if (schedule_ == new_schedule) {
    // Do not schedule if nothing has changed;
    return;
  }

  schedule_ = new_schedule;
  StoreFetchingSchedule();
  ApplyPersistentFetchingSchedule();
}

void RemoteSuggestionsSchedulerImpl::StopScheduling() {
  if (schedule_.is_empty()) {
    // Do not unschedule if already switched off.
    return;
  }

  schedule_ = FetchingSchedule::Empty();
  StoreFetchingSchedule();
  ApplyPersistentFetchingSchedule();
}

void RemoteSuggestionsSchedulerImpl::ApplyPersistentFetchingSchedule() {
  // The scheduler only exists on Android so far, it's null on other platforms.
  if (persistent_scheduler_) {
    if (schedule_.is_empty()) {
      persistent_scheduler_->Unschedule();
    } else {
      persistent_scheduler_->Schedule(schedule_.interval_persistent_wifi,
                                      schedule_.interval_persistent_fallback);
    }
  }
}

RemoteSuggestionsSchedulerImpl::FetchingSchedule
RemoteSuggestionsSchedulerImpl::GetDesiredFetchingSchedule() const {
  UserClassifier::UserClass user_class = user_classifier_->GetUserClass();

  FetchingSchedule schedule;
  schedule.interval_persistent_wifi =
      GetDesiredFetchingInterval(FetchingInterval::PERSISTENT_WIFI, user_class);
  schedule.interval_persistent_fallback = GetDesiredFetchingInterval(
      FetchingInterval::PERSISTENT_FALLBACK, user_class);
  schedule.interval_startup_wifi =
      GetDesiredFetchingInterval(FetchingInterval::STARTUP_WIFI, user_class);
  schedule.interval_startup_fallback = GetDesiredFetchingInterval(
      FetchingInterval::STARTUP_FALLBACK, user_class);
  schedule.interval_shown_wifi =
      GetDesiredFetchingInterval(FetchingInterval::SHOWN_WIFI, user_class);
  schedule.interval_shown_fallback =
      GetDesiredFetchingInterval(FetchingInterval::SHOWN_FALLBACK, user_class);

  return schedule;
}

void RemoteSuggestionsSchedulerImpl::LoadLastFetchingSchedule() {
  schedule_.interval_persistent_wifi = profile_prefs_->GetTimeDelta(
      prefs::kSnippetPersistentFetchingIntervalWifi);
  schedule_.interval_persistent_fallback = profile_prefs_->GetTimeDelta(
      prefs::kSnippetPersistentFetchingIntervalFallback);
  schedule_.interval_startup_wifi =
      profile_prefs_->GetTimeDelta(prefs::kSnippetStartupFetchingIntervalWifi);
  schedule_.interval_startup_fallback = profile_prefs_->GetTimeDelta(
      prefs::kSnippetStartupFetchingIntervalFallback);
  schedule_.interval_shown_wifi =
      profile_prefs_->GetTimeDelta(prefs::kSnippetShownFetchingIntervalWifi);
  schedule_.interval_shown_fallback = profile_prefs_->GetTimeDelta(
      prefs::kSnippetShownFetchingIntervalFallback);
}

void RemoteSuggestionsSchedulerImpl::StoreFetchingSchedule() {
  profile_prefs_->SetTimeDelta(prefs::kSnippetPersistentFetchingIntervalWifi,
                               schedule_.interval_persistent_wifi);
  profile_prefs_->SetTimeDelta(
      prefs::kSnippetPersistentFetchingIntervalFallback,
      schedule_.interval_persistent_fallback);
  profile_prefs_->SetTimeDelta(prefs::kSnippetStartupFetchingIntervalWifi,
                               schedule_.interval_startup_wifi);
  profile_prefs_->SetTimeDelta(prefs::kSnippetStartupFetchingIntervalFallback,
                               schedule_.interval_startup_fallback);
  profile_prefs_->SetTimeDelta(prefs::kSnippetShownFetchingIntervalWifi,
                               schedule_.interval_shown_wifi);
  profile_prefs_->SetTimeDelta(prefs::kSnippetShownFetchingIntervalFallback,
                               schedule_.interval_shown_fallback);
}

bool RemoteSuggestionsSchedulerImpl::IsLastSuccessfulFetchStale() const {
  // Avoid claiming staleness on the first fetch ever (after installing /
  // upgrading Chrome to a version that writes this pref). We really do not
  // know when was the last fetch.
  if (!profile_prefs_->HasPrefPath(prefs::kSnippetLastSuccessfulFetchTime)) {
    return false;
  }
  const base::Time last_successful_fetch_time =
      profile_prefs_->GetTime(prefs::kSnippetLastSuccessfulFetchTime);

  return clock_->Now() - last_successful_fetch_time >
         schedule_.GetStalenessInterval();
}

void RemoteSuggestionsSchedulerImpl::RefetchIfAppropriate(TriggerType trigger) {

  if (background_fetch_in_progress_) {
    return;
  }

  if (enabled_triggers_.count(trigger) == 0) {
    return;
  }

  if (net::NetworkChangeNotifier::IsOffline()) {
    // Do not let a request fail due to lack of internet connection. Then, such
    // a failure would get logged and further requests would be blocked for a
    // while (even after becoming online).
    return;
  }

  if (!IsReadyForBackgroundFetches()) {
    queued_triggers_.insert(trigger);
    return;
  }

  const base::Time last_fetch_attempt_time =
      profile_prefs_->GetTime(prefs::kSnippetLastFetchAttemptTime);

  if (trigger == TriggerType::SURFACE_OPENED &&
      !time_until_first_shown_trigger_reported_) {
    time_until_first_shown_trigger_reported_ = true;
    ReportTimeUntilFirstShownTrigger(user_classifier_->GetUserClass(),
                                     clock_->Now() - last_fetch_attempt_time);
  }

  if ((trigger == TriggerType::BROWSER_FOREGROUNDED ||
       trigger == TriggerType::BROWSER_COLD_START) &&
      !time_until_first_startup_trigger_reported_) {
    time_until_first_startup_trigger_reported_ = true;
    ReportTimeUntilFirstStartupTrigger(user_classifier_->GetUserClass(),
                                       clock_->Now() - last_fetch_attempt_time);
  }

  if (trigger != TriggerType::PERSISTENT_SCHEDULER_WAKE_UP &&
      !ShouldRefetchNow(last_fetch_attempt_time, trigger)) {
    return;
  }

  if (!AcquireQuota(/*interactive_request=*/false)) {
    return;
  }

  base::TimeDelta diff = clock_->Now() - last_fetch_attempt_time;
  switch (trigger) {
    case TriggerType::PERSISTENT_SCHEDULER_WAKE_UP:
      ReportTimeUntilPersistentFetch(user_classifier_->GetUserClass(), diff);
      break;
    case TriggerType::SURFACE_OPENED:
      ReportTimeUntilShownFetch(user_classifier_->GetUserClass(), diff);
      break;
    case TriggerType::BROWSER_FOREGROUNDED:
    case TriggerType::BROWSER_COLD_START:
      ReportTimeUntilStartupFetch(user_classifier_->GetUserClass(), diff);
      break;
    case TriggerType::COUNT:
      NOTREACHED();
  }

  UMA_HISTOGRAM_ENUMERATION(
      "NewTabPage.ContentSuggestions.BackgroundFetchTrigger",
      static_cast<int>(trigger), static_cast<int>(TriggerType::COUNT));

  background_fetch_in_progress_ = true;

  if ((trigger == TriggerType::BROWSER_COLD_START ||
       trigger == TriggerType::BROWSER_FOREGROUNDED ||
       trigger == TriggerType::SURFACE_OPENED) &&
      IsLastSuccessfulFetchStale()) {
    provider_->RefetchWhileDisplaying(
        base::BindOnce(&RemoteSuggestionsSchedulerImpl::RefetchFinished,
                       base::Unretained(this)));
    return;
  }

  provider_->RefetchInTheBackground(
      base::BindOnce(&RemoteSuggestionsSchedulerImpl::RefetchFinished,
                     base::Unretained(this)));
}

bool RemoteSuggestionsSchedulerImpl::ShouldRefetchNow(
    base::Time last_fetch_attempt_time,
    TriggerType trigger) {
  // If we have no persistent scheduler to ask, err on the side of caution.
  bool wifi = false;
  if (persistent_scheduler_) {
    wifi = persistent_scheduler_->IsOnUnmeteredConnection();
  }

  base::Time first_allowed_fetch_time = last_fetch_attempt_time;
  if (trigger == TriggerType::SURFACE_OPENED) {
    first_allowed_fetch_time += (wifi ? schedule_.interval_shown_wifi
                                      : schedule_.interval_shown_fallback);
  } else {
    first_allowed_fetch_time += (wifi ? schedule_.interval_startup_wifi
                                      : schedule_.interval_startup_fallback);
  }

  base::Time now = clock_->Now();

  return background_fetches_allowed_after_ <= now &&
         first_allowed_fetch_time <= now;
}

bool RemoteSuggestionsSchedulerImpl::IsReadyForBackgroundFetches() const {
  if (!provider_ || !provider_->ready()) {
    return false;  // Cannot fetch as remote suggestions provider does not
                   // exist or is not active yet.
  }

  if (schedule_.is_empty()) {
    return false;  // Background fetches are disabled in general.
  }
  if (!eula_state_->IsEulaAccepted()) {
    return false;  // No background fetches are allowed before EULA is accepted.
  }

  return true;
}

bool RemoteSuggestionsSchedulerImpl::AcquireQuota(bool interactive_request) {
  switch (user_classifier_->GetUserClass()) {
    case UserClassifier::UserClass::RARE_NTP_USER:
      return request_throttler_rare_ntp_user_.DemandQuotaForRequest(
          interactive_request);
    case UserClassifier::UserClass::ACTIVE_NTP_USER:
      return request_throttler_active_ntp_user_.DemandQuotaForRequest(
          interactive_request);
    case UserClassifier::UserClass::ACTIVE_SUGGESTIONS_CONSUMER:
      return request_throttler_active_suggestions_consumer_
          .DemandQuotaForRequest(interactive_request);
  }
  NOTREACHED();
  return false;
}

void RemoteSuggestionsSchedulerImpl::RefetchFinished(Status fetch_status) {
  background_fetch_in_progress_ = false;
  OnFetchCompleted(fetch_status);
}

void RemoteSuggestionsSchedulerImpl::OnFetchCompleted(Status fetch_status) {
  profile_prefs_->SetTime(prefs::kSnippetLastFetchAttemptTime, clock_->Now());
  time_until_first_shown_trigger_reported_ = false;
  time_until_first_startup_trigger_reported_ = false;

  // Reschedule after a fetch. The persistent schedule is applied only after a
  // successful fetch. After a failed fetch, we want to keep the previous
  // persistent schedule intact so that we eventually get a persistent
  // fallback fetch (if the wifi persistent fetches keep failing).
  if (fetch_status.code != StatusCode::SUCCESS) {
    return;
  }

  profile_prefs_->SetTime(prefs::kSnippetLastSuccessfulFetchTime,
                          clock_->Now());

  ApplyPersistentFetchingSchedule();
}

void RemoteSuggestionsSchedulerImpl::ClearLastFetchAttemptTime() {
  // Added during Feed rollout to help investigate https://crbug.com/908963.
  base::TimeDelta attempt_age =
      clock_->Now() -
      profile_prefs_->GetTime(prefs::kSnippetLastFetchAttemptTime);
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "ContentSuggestions.Feed.Scheduler.TimeSinceLastFetchOnClear",
      attempt_age, base::TimeDelta::FromSeconds(1),
      base::TimeDelta::FromDays(7),
      /*bucket_count=*/50);

  profile_prefs_->ClearPref(prefs::kSnippetLastFetchAttemptTime);
  // To mark the last fetch as stale, we need to keep the time in prefs, only
  // making sure it is long ago.
  profile_prefs_->SetTime(prefs::kSnippetLastSuccessfulFetchTime, base::Time());
}

std::set<RemoteSuggestionsSchedulerImpl::TriggerType>
RemoteSuggestionsSchedulerImpl::GetEnabledTriggerTypes() {
  static_assert(static_cast<unsigned int>(TriggerType::COUNT) ==
                    base::size(kTriggerTypeNames),
                "Fill in names for trigger types.");

  std::string param_value = base::GetFieldTrialParamValueByFeature(
      ntp_snippets::kArticleSuggestionsFeature, kTriggerTypesParamName);
  if (param_value == kTriggerTypesParamValueForEmptyList) {
    return std::set<TriggerType>();
  }

  std::vector<std::string> tokens = base::SplitString(
      param_value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (tokens.empty()) {
    return GetDefaultEnabledTriggerTypes();
  }

  std::set<TriggerType> enabled_types;
  for (const auto& token : tokens) {
    auto* const* it = std::find(std::begin(kTriggerTypeNames),
                                std::end(kTriggerTypeNames), token);
    if (it == std::end(kTriggerTypeNames)) {
      DLOG(WARNING) << "Failed to parse variation param "
                    << kTriggerTypesParamName << " with string value "
                    << param_value
                    << " into a comma-separated list of keywords. "
                    << "Unknown token " << token
                    << " found. Falling back to default value.";
      return GetDefaultEnabledTriggerTypes();
    }

    // Add the enabled type represented by |token| into the result set.
    enabled_types.insert(
        static_cast<TriggerType>(it - std::begin(kTriggerTypeNames)));
  }
  return enabled_types;
}

std::set<RemoteSuggestionsSchedulerImpl::TriggerType>
RemoteSuggestionsSchedulerImpl::GetDefaultEnabledTriggerTypes() {
  return {TriggerType::PERSISTENT_SCHEDULER_WAKE_UP,
          TriggerType::SURFACE_OPENED, TriggerType::BROWSER_COLD_START,
          TriggerType::BROWSER_FOREGROUNDED};
}

}  // namespace ntp_snippets
