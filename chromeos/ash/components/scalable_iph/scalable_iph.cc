// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/scalable_iph/scalable_iph.h"

#include "base/no_destructor.h"

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
const std::vector<const base::Feature*>& GetFeatureList() {
  static const base::NoDestructor<std::vector<const base::Feature*>>
      feature_list({});
  return *feature_list;
}

}  // namespace

ScalableIph::ScalableIph(feature_engagement::Tracker* tracker)
    : tracker_(tracker) {
  CHECK(tracker_);
}

ScalableIph::~ScalableIph() = default;

void ScalableIph::Shutdown() {
  tracker_ = nullptr;
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
      // TODO(b/284053005): Call the UI framework to trigger a help UI.
    }
  }
}

}  // namespace scalable_iph
