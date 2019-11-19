// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_TRACKER_IMPL_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_TRACKER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "components/feature_engagement/public/tracker.h"

namespace feature_engagement {
class AvailabilityModel;
class Configuration;
class ConditionValidator;
class DisplayLockController;
class DisplayLockHandle;
class EventModel;
class TimeProvider;

// The internal implementation of the Tracker.
class TrackerImpl : public Tracker, public base::SupportsUserData {
 public:
  TrackerImpl(std::unique_ptr<EventModel> event_model,
              std::unique_ptr<AvailabilityModel> availability_model,
              std::unique_ptr<Configuration> configuration,
              std::unique_ptr<DisplayLockController> display_lock_controller,
              std::unique_ptr<ConditionValidator> condition_validator,
              std::unique_ptr<TimeProvider> time_provider);
  ~TrackerImpl() override;

  // Tracker implementation.
  void NotifyEvent(const std::string& event) override;
  bool ShouldTriggerHelpUI(const base::Feature& feature) override;
  bool WouldTriggerHelpUI(const base::Feature& feature) const override;
  Tracker::TriggerState GetTriggerState(
      const base::Feature& feature) const override;
  void Dismissed(const base::Feature& feature) override;
  std::unique_ptr<DisplayLockHandle> AcquireDisplayLock() override;
  bool IsInitialized() const override;
  void AddOnInitializedCallback(OnInitializedCallback callback) override;

 private:
  // Invoked by the EventModel when it has been initialized.
  void OnEventModelInitializationFinished(bool success);

  // Invoked by the AvailabilityModel when it has been initialized.
  void OnAvailabilityModelInitializationFinished(bool success);

  // Returns whether both underlying models have finished initializing.
  // This returning true does not mean the initialization was a success, just
  // that it is finished.
  bool IsInitializationFinished() const;

  // Posts the results to the OnInitializedCallbacks if
  // IsInitializationFinished() returns true.
  void MaybePostInitializedCallbacks();

  // The current model for all events.
  std::unique_ptr<EventModel> event_model_;

  // The current model for when particular features were enabled.
  std::unique_ptr<AvailabilityModel> availability_model_;

  // The current configuration for all features.
  std::unique_ptr<Configuration> configuration_;

  // The DisplayLockController provides functionality for letting API users hold
  // a lock to ensure no feature enlightenment is happening while any lock is
  // held.
  std::unique_ptr<DisplayLockController> display_lock_controller_;

  // The ConditionValidator provides functionality for knowing when to trigger
  // help UI.
  std::unique_ptr<ConditionValidator> condition_validator_;

  // A utility for retriving time-related information.
  std::unique_ptr<TimeProvider> time_provider_;

  // Whether the initialization of the underlying EventModel has finished.
  bool event_model_initialization_finished_;

  // Whether the initialization of the underlying AvailabilityModel has
  // finished.
  bool availability_model_initialization_finished_;

  // The list of callbacks to invoke when initialization has finished. This
  // is cleared after the initialization has happened.
  std::vector<OnInitializedCallback> on_initialized_callbacks_;

  base::WeakPtrFactory<TrackerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TrackerImpl);
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_TRACKER_IMPL_H_
