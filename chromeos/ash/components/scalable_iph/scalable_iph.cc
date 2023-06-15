// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/scalable_iph/scalable_iph.h"

#include <memory>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "chromeos/ash/components/scalable_iph/iph_session.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"

namespace scalable_iph {

namespace {

constexpr char kFunctionCallAfterKeyedServiceShutdown[] =
    "Function call after keyed service shutdown.";

const std::map<ScalableIph::Event, std::string>& GetEventNamesMap() {
  // IPH events are put in a global namespace. Prefix with ScalableIph for all
  // events.
  static const base::NoDestructor<std::map<ScalableIph::Event, std::string>>
      event_names_map(
          {{ScalableIph::Event::kFiveMinTick, "ScalableIphFiveMinTick"}});
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

}  // namespace

ScalableIph::ScalableIph(feature_engagement::Tracker* tracker,
                         std::unique_ptr<ScalableIphDelegate> delegate)
    : tracker_(tracker), delegate_(std::move(delegate)) {
  CHECK(tracker_);
  CHECK(delegate_);

  EnsureTimerStarted();
}

ScalableIph::~ScalableIph() = default;

void ScalableIph::Shutdown() {
  timer_.Stop();

  tracker_ = nullptr;
  delegate_.reset();
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

void ScalableIph::CheckTriggerConditions() {
  // Make sure that `tracker_` is initialized. `tracker_` should not cause crash
  // even if we call `ShouldTriggerHelpUI` before initialization. But it returns
  // false. It can become a difficult to notice/debug bug if we accidentally
  // introduce a code path where we call it before initialization.
  DCHECK(tracker_->IsInitialized());

  for (const base::Feature* feature : GetFeatureList()) {
    if (tracker_->ShouldTriggerHelpUI(*feature)) {
      // TODO(b/284053005): Add the actual implementations.
      ScalableIphDelegate::BubbleParams params;
      delegate_->ShowBubble(params,
                            std::make_unique<IphSession>(*feature, tracker_));
    }
  }
}

const std::vector<const base::Feature*>& ScalableIph::GetFeatureList() const {
  if (!feature_list_for_testing_.empty()) {
    return feature_list_for_testing_;
  }

  return GetFeatureListConstant();
}

}  // namespace scalable_iph
