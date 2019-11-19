// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_FEED_SCHEDULER_HOST_H_
#define COMPONENTS_FEED_CORE_FEED_SCHEDULER_HOST_H_

#include <memory>
#include <set>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/feed/core/refresh_throttler.h"
#include "components/feed/core/user_classifier.h"
#include "components/web_resource/eula_accepted_notifier.h"

class PrefService;

namespace base {
class Clock;
class Time;
class TimeDelta;
}  // namespace base

namespace feed {

// The enum values and names are kept in sync with SchedulerApi.RequestBehavior
// through Java unit tests, new values however must be manually added. If any
// new values are added, also update FeedSchedulerBridgeTest.java as well as
// the corresponding definition in enums.xml.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.feed
enum NativeRequestBehavior {
  kUnknown = 0,
  kRequestWithWait = 1,
  kRequestWithContent = 2,
  kRequestWithTimeout = 3,
  kNoRequestWithWait = 4,
  kNoRequestWithContent = 5,
  kNoRequestWithTimeout = 6,
  kMaxValue = kNoRequestWithTimeout
};

// Implementation of the Feed Scheduler Host API. The scheduler host decides
// what content is allowed to be shown, based on its age, and when to fetch new
// content.
class FeedSchedulerHost : web_resource::EulaAcceptedNotifier::Observer {
 public:
  // The TriggerType enum specifies values for the events that can trigger
  // refreshing articles. When adding values, be certain to also update the
  // corresponding definition in enums.xml.
  enum class TriggerType {
    kNtpShown = 0,
    kForegrounded = 1,
    kFixedTimer = 2,
    kMaxValue = kFixedTimer
  };

  // Enum for the status of the refresh, reported through UMA.
  // If any new values are added, update the corresponding definition in
  // enums.xml.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum ShouldRefreshResult {
    kShouldRefresh = 0,
    kDontRefreshOutstandingRequest = 1,
    kDontRefreshTriggerDisabled = 2,
    kDontRefreshNetworkOffline = 3,
    kDontRefreshEulaNotAccepted = 4,
    kDontRefreshArticlesHidden = 5,
    kDontRefreshRefreshSuppressed = 6,
    kDontRefreshNotStale = 7,
    kDontRefreshRefreshThrottled = 8,
    kMaxValue = kDontRefreshRefreshThrottled,
  };

  FeedSchedulerHost(PrefService* profile_prefs,
                    PrefService* local_state,
                    base::Clock* clock);
  ~FeedSchedulerHost() override;

  using ScheduleBackgroundTaskCallback =
      base::RepeatingCallback<void(base::TimeDelta)>;

  // Provide dependent pieces of functionality the scheduler relies on. Should
  // be called exactly once before other public methods are called. This is
  // separate from the constructor because the FeedHostService owns and creates
  // this class, while these providers need to be bridged through the JNI into a
  // specific place that the FeedHostService does not know of.
  void Initialize(
      base::RepeatingClosure refresh_callback,
      ScheduleBackgroundTaskCallback schedule_background_task_callback,
      base::RepeatingClosure cancel_background_task_callback);

  // Called when the NTP is opened to decide how to handle displaying and
  // refreshing content.
  NativeRequestBehavior ShouldSessionRequestData(
      bool has_content,
      base::Time content_creation_date_time,
      bool has_outstanding_request);

  // Called when a successful refresh completes.
  void OnReceiveNewContent(base::Time content_creation_date_time);

  // Called when an unsuccessful refresh completes.
  void OnRequestError(int network_response_code);

  // Called when browser is opened, launched, or foregrounded.
  void OnForegrounded();

  // Called when the scheduled fixed timer wakes up, |on_completion| will be
  // invoked when the refresh completes, regardless of success. If no refresh
  // is started for this trigger, |on_completion| is run immediately.
  void OnFixedTimer(base::OnceClosure on_completion);

  // Should be called when a suggestion is consumed. This is a signal the
  // scheduler users to track the kind of user, and optimize refresh frequency.
  void OnSuggestionConsumed();

  // Should be called when suggestions are shown. This is a signal the scheduler
  // users to track the kind of user, and optimize refresh frequency.
  void OnSuggestionsShown();

  // Should be called when something happens to clear stored articles. The
  // scheduler updates its internal state and treats this event as a kNtpShown
  // trigger. Similar to ShouldSessionRequestData(), the scheduler will not
  // start a refresh itself during this method. Instead, the caller should check
  // the return value, and if true, the caller should start a refresh.
  bool OnArticlesCleared(bool suppress_refreshes);

  // Surface user_classifier_ for internals debugging page.
  UserClassifier* GetUserClassifierForDebugging();

  // Surface suppress_refreshes_until_ for internals debugging page.
  base::Time GetSuppressRefreshesUntilForDebugging() const;

  // Surface last_fetch_status_ for internals debugging page.
  int GetLastFetchStatusForDebugging() const;

  // Surface the TriggerType for the last ShouldRefresh check that resulted in
  // kShouldRefresh. Callers of ShouldRefresh are presumed to follow with the
  // actual refresh.
  TriggerType* GetLastFetchTriggerTypeForDebugging() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(FeedSchedulerHostTest, GetTriggerThreshold);

  // web_resource::EulaAcceptedNotifier::Observer:
  void OnEulaAccepted() override;

  // Determines whether a refresh should be performed for the given |trigger|.
  // If this method is called and returns kShouldRefresh we presume the refresh
  // will happen, therefore we report metrics respectively and update
  // |tracking_oustanding_request_|.
  ShouldRefreshResult ShouldRefresh(TriggerType trigger);

  // Decides if content whose age is the difference between now and
  // |content_creation_date_time| is old enough to be considered stale.
  bool IsContentStale(base::Time content_creation_date_time);

  // Returns the time threshold for content or previous refresh attempt to be
  // considered old enough for a given trigger to warrant a refresh.
  base::TimeDelta GetTriggerThreshold(TriggerType trigger);

  // Schedules a task to wake up and try to refresh. Overrides previously
  // scheduled tasks.
  void ScheduleFixedTimerWakeUp(base::TimeDelta period);

  // Clears the task to wake up and try to refresh.
  void CancelFixedTimerWakeUp();

  // Non-owning reference to pref service providing durable storage.
  PrefService* profile_prefs_;

  // Non-owning reference to clock to get current time.
  base::Clock* clock_;

  // Persists NTP and article usage over time and provides a classification.
  UserClassifier user_classifier_;

  // Callback to request that an async refresh be started.
  base::RepeatingClosure refresh_callback_;

  // Provides ability to schedule persistent background fixed timer wake ups
  // that will call into OnFixedTimer().
  ScheduleBackgroundTaskCallback schedule_background_task_callback_;

  // Provides ability to cancel the persistent background fixed timer wake ups.
  base::RepeatingClosure cancel_background_task_callback_;

  // When a background wake up has caused a fixed timer refresh, this callback
  // will be valid and holds a way to inform the task driving this wake up that
  // the refresh has completed. Is called on refresh success or failure.
  base::OnceClosure fixed_timer_completion_;

  // Set of triggers that should be ignored. By default this is empty.
  std::set<TriggerType> disabled_triggers_;

  // In some circumstances, such as when history is cleared, the scheduler will
  // stop requesting refreshes for a given period. During this time, only direct
  // user interaction with the NTP (and outside of the scheduler's control)
  // should cause a refresh to occur.
  base::Time suppress_refreshes_until_;

  // The goal of this field is to not make multiple refresh request at the same
  // time. When the scheduler starts or indicates the caller should start a
  // request, this field is set. When that request finishes, this field is
  // cleared. It is unclear if this field is always and correctly cleared out,
  // so after the point in time held by this field, the scheduler is allowed to
  // trigger another request.
  base::Time outstanding_request_until_;

  // May hold a nullptr if the platform does not show the user a EULA. Will only
  // notify if IsEulaAccepted() is called and it returns false.
  std::unique_ptr<web_resource::EulaAcceptedNotifier> eula_accepted_notifier_;

  // Variables to allow metrics to be reported the first time a given trigger
  // occurs after a refresh.
  bool time_until_first_shown_trigger_reported_ = false;
  bool time_until_first_foregrounded_trigger_reported_ = false;

  // In the case the user transitions between user classes, hold onto a
  // throttler for any situation.
  base::flat_map<UserClassifier::UserClass, std::unique_ptr<RefreshThrottler>>
      throttlers_;

  // Status of the last fetch for debugging.
  int last_fetch_status_ = 0;

  // Reason for last fetch for debugging.
  std::unique_ptr<TriggerType> last_fetch_trigger_type_;

  DISALLOW_COPY_AND_ASSIGN(FeedSchedulerHost);
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_FEED_SCHEDULER_HOST_H_
