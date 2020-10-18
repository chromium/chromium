// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_ANDROID_WRAPPING_TEST_TRACKER_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_ANDROID_WRAPPING_TEST_TRACKER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "components/feature_engagement/internal/tracker_impl.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#endif  // defined(OS_ANDROID)

namespace feature_engagement {
class AvailabilityModel;
class Configuration;
class ConditionValidator;
class DisplayLockController;
class EventModel;
class TimeProvider;

// This class is a thin wrapper around TrackerImpl and has two modes of
// operating. To start with, it does nothing but forward all calls to its base
// class implementation. However, at some point a Java test can inject a
// tracker, at
// which point this class will start to forward the calls to Java. See
// InjectTracker for details.
class WrappingTestTracker : public TrackerImpl {
 public:
  WrappingTestTracker(
      std::unique_ptr<EventModel> event_model,
      std::unique_ptr<AvailabilityModel> availability_model,
      std::unique_ptr<Configuration> configuration,
      std::unique_ptr<DisplayLockController> display_lock_controller,
      std::unique_ptr<ConditionValidator> condition_validator,
      std::unique_ptr<TimeProvider> time_provider);
  ~WrappingTestTracker() override;

  // Injects a tracker from Java to proxy calls to. By default, all functions
  // will be forwarded to the base class for processing until this function is
  // called, at which point the proxying calls to Java starts. Note that not all
  // functions are proxied to Java. Some are still handled by the base class.
  // See .cc for details.
  void InjectTracker(const base::android::JavaRef<jobject>& jtracker);

  // TrackerImpl:

  void NotifyEvent(const std::string& event) override;
  bool ShouldTriggerHelpUI(const base::Feature& feature) override;
  bool WouldTriggerHelpUI(const base::Feature& feature) const override;
  bool HasEverTriggered(const base::Feature& feature,
                        bool from_window) const override;
  TriggerState GetTriggerState(const base::Feature& feature) const override;
  void Dismissed(const base::Feature& feature) override;
  std::unique_ptr<DisplayLockHandle> AcquireDisplayLock() override;
  bool IsInitialized() const override;
  void AddOnInitializedCallback(OnInitializedCallback callback) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_tracker_;

  DISALLOW_COPY_AND_ASSIGN(WrappingTestTracker);
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_ANDROID_WRAPPING_TEST_TRACKER_H_
