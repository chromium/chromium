// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/scalable_iph/scalable_iph.h"

#include <memory>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/scalable_iph/iph_session.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_constants.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"

namespace scalable_iph {

namespace {

using NotificationParams =
    ::scalable_iph::ScalableIphDelegate::NotificationParams;

constexpr char kFunctionCallAfterKeyedServiceShutdown[] =
    "Function call after keyed service shutdown.";

const std::map<ScalableIph::Event, std::string>& GetEventNamesMap() {
  // IPH events are put in a global namespace. Prefix with ScalableIph for all
  // events.
  static const base::NoDestructor<std::map<ScalableIph::Event, std::string>>
      event_names_map(
          {{ScalableIph::Event::kFiveMinTick, kEventNameFiveMinTick}});
  return *event_names_map;
}

// The list of IPH features `SclableIph` supports. `ScalableIph` checks trigger
// conditions of all events listed in this list when it receives an `Event`.
const std::vector<const base::Feature*>& GetFeatureListConstant() {
  static const base::NoDestructor<std::vector<const base::Feature*>>
      feature_list({});
  return *feature_list;
}

constexpr base::TimeDelta kTimeTickEventInterval = base::Minutes(5);

UiType ParseUiType(const base::Feature& feature) {
  std::string ui_type =
      base::GetFieldTrialParamValueByFeature(feature, kCustomUiTypeParamName);
  CHECK(ui_type == kCustomUiTypeValueNotification);
  return UiType::kNotification;
}

NotificationParams ParseNotificationParams(const base::Feature& feature) {
  // TODO(b/288167957): Implement a fallback for an invalid config, e.g. Do not
  // show an IPH for the case instead of CHECK failure. Config is served from
  // the server. This is not a constraint coming from client side.
  NotificationParams param;
  param.title = base::GetFieldTrialParamValueByFeature(
      feature, kCustomNotificationTitleParamName);
  CHECK(!param.title.empty())
      << kCustomNotificationTitleParamName << " is a required field";
  param.text = base::GetFieldTrialParamValueByFeature(
      feature, kCustomNotificationBodyTextParamName);
  CHECK(!param.text.empty())
      << kCustomNotificationBodyTextParamName << " is a required field";
  param.button.text = base::GetFieldTrialParamValueByFeature(
      feature, kCustomNotificationButtonTextParamName);
  CHECK(!param.button.text.empty())
      << kCustomNotificationButtonTextParamName << " is a required field";
  return param;
}

}  // namespace

ScalableIph::ScalableIph(feature_engagement::Tracker* tracker,
                         std::unique_ptr<ScalableIphDelegate> delegate)
    : tracker_(tracker), delegate_(std::move(delegate)) {
  CHECK(tracker_);
  CHECK(delegate_);

  delegate_observation_.Observe(delegate_.get());

  EnsureTimerStarted();

  online_ = delegate_->IsOnline();

  tracker_->AddOnInitializedCallback(
      base::BindOnce(&ScalableIph::CheckTriggerConditionsOnInitSuccess,
                     weak_ptr_factory_.GetWeakPtr()));
}

ScalableIph::~ScalableIph() = default;

void ScalableIph::Shutdown() {
  timer_.Stop();

  tracker_ = nullptr;

  delegate_observation_.Reset();
  delegate_.reset();
}

void ScalableIph::OnConnectionChanged(bool online) {
  if (online_ == online) {
    return;
  }

  online_ = online;

  tracker_->AddOnInitializedCallback(
      base::BindOnce(&ScalableIph::CheckTriggerConditionsOnInitSuccess,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ScalableIph::OverrideFeatureListForTesting(
    const std::vector<const base::Feature*> feature_list) {
  CHECK(feature_list_for_testing_.size() == 0)
      << "It's NOT allowed to override feature list twice for testing";
  CHECK(feature_list.size() > 0) << "An empty list is NOT allowed to set.";

  feature_list_for_testing_ = feature_list;
}

void ScalableIph::OverrideTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  CHECK(timer_.IsRunning())
      << "Timer is expected to be always running until Shutdown";
  timer_.Stop();
  timer_.SetTaskRunner(task_runner);
  EnsureTimerStarted();
}

void ScalableIph::PerformAction(ActionType action_type) {
  // TODO(b/289108135): Implement this.
  delegate_->PerformActionForScalableIph(action_type);
}

void ScalableIph::RecordEvent(ScalableIph::Event event) {
  if (!tracker_) {
    DCHECK(false) << kFunctionCallAfterKeyedServiceShutdown;
    return;
  }

  // `AddOnInitializedCallback` immediately calls the callback if it's already
  // initialized.
  tracker_->AddOnInitializedCallback(
      base::BindOnce(&ScalableIph::RecordEventInternal,
                     weak_ptr_factory_.GetWeakPtr(), event));
}

void ScalableIph::EnsureTimerStarted() {
  timer_.Start(FROM_HERE, kTimeTickEventInterval,
               base::BindRepeating(&ScalableIph::RecordTimeTickEvent,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void ScalableIph::RecordTimeTickEvent() {
  RecordEvent(Event::kFiveMinTick);
}

void ScalableIph::RecordEventInternal(ScalableIph::Event event,
                                      bool init_success) {
  if (!tracker_) {
    DCHECK(false) << kFunctionCallAfterKeyedServiceShutdown;
    return;
  }

  if (!init_success) {
    DCHECK(false) << "Failed to initialize feature_engagement::Tracker.";
    return;
  }

  auto it = GetEventNamesMap().find(event);
  if (it == GetEventNamesMap().end()) {
    DCHECK(false) << "Missing ScalableIph::Event to event name string mapping.";
    return;
  }

  tracker_->NotifyEvent(it->second);

  CheckTriggerConditions();
}

void ScalableIph::CheckTriggerConditionsOnInitSuccess(bool init_success) {
  if (!init_success) {
    DCHECK(false) << "Failed to initialize feature_engagement::Tracker.";
    return;
  }

  CheckTriggerConditions();
}

void ScalableIph::CheckTriggerConditions() {
  // Make sure that `tracker_` is initialized. `tracker_` should not cause crash
  // even if we call `ShouldTriggerHelpUI` before initialization. But it returns
  // false. It can become a difficult to notice/debug bug if we accidentally
  // introduce a code path where we call it before initialization.
  DCHECK(tracker_->IsInitialized());

  for (const base::Feature* feature : GetFeatureList()) {
    // TODO(b/289267799): Check our custom extension version number.
    if (CheckCustomConditions(*feature) &&
        tracker_->ShouldTriggerHelpUI(*feature)) {
      // TODO(b/289286456): Set up browser test and clean up.
      if (show_notification_for_testing_) {
        // Only show notification once in the test for now.
        show_notification_for_testing_ = false;
        ScalableIphDelegate::NotificationParams notification_params;
        notification_params.type =
            ScalableIphDelegate::NotificationType::kWallpaper;
        notification_params.button.text = "test";
        delegate_->ShowNotification(
            notification_params,
            std::make_unique<IphSession>(*feature, tracker_));
      } else {
        // TODO(b/284053005): Support other ui types.
        UiType ui_type = ParseUiType(*feature);
        CHECK(ui_type == UiType::kNotification)
            << "Only Notification is implemented now";
        delegate_->ShowNotification(
            ParseNotificationParams(*feature),
            std::make_unique<IphSession>(*feature, tracker_));
      }
    }
  }
}

bool ScalableIph::CheckCustomConditions(const base::Feature& feature) {
  return CheckNetworkConnection(feature) && CheckClientAge(feature);
}

bool ScalableIph::CheckNetworkConnection(const base::Feature& feature) {
  std::string connection_condition = base::GetFieldTrialParamValueByFeature(
      feature, kCustomConditionNetworkConnectionParamName);
  if (connection_condition.empty()) {
    return true;
  }

  // If an invalid value is provided, does not satisfy a condition for a
  // fail-safe behavior.
  if (connection_condition != kCustomConditionNetworkConnectionOnline) {
    DLOG(WARNING) << "Only " << kCustomConditionNetworkConnectionOnline
                  << " is the valid value for network connection condition";
    return false;
  }

  return online_;
}

bool ScalableIph::CheckClientAge(const base::Feature& feature) {
  std::string client_age_condition = base::GetFieldTrialParamValueByFeature(
      feature, kCustomConditionClientAgeInDaysParamName);
  if (client_age_condition.empty()) {
    return true;
  }

  // Use `DLOG`s for logging instead of `DCHECK(false)` as we want to test those
  // fail-safe behaviors in browser_tests.
  int max_client_age = 0;
  if (!base::StringToInt(client_age_condition, &max_client_age)) {
    DLOG(WARNING)
        << "Failed to parse client age condition. It must be an integer.";
    return false;
  }

  if (max_client_age < 0) {
    DLOG(WARNING) << "Client age condition must be a positive integer value.";
    return false;
  }

  int client_age = delegate_->ClientAgeInDays();
  if (client_age < 0) {
    DLOG(WARNING) << "Client age is a negative number. This can happen if a "
                     "user changes time zone, etc. Condition is not satisfied "
                     "for a fail safe behavior.";
    return false;
  }

  return client_age <= max_client_age;
}

const std::vector<const base::Feature*>& ScalableIph::GetFeatureList() const {
  if (!feature_list_for_testing_.empty()) {
    return feature_list_for_testing_;
  }

  return GetFeatureListConstant();
}

}  // namespace scalable_iph
