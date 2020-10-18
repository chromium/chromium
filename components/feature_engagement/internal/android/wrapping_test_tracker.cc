// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/android/wrapping_test_tracker.h"

#include <memory>
#include <utility>

#include "base/android/jni_string.h"
#include "components/feature_engagement/internal/availability_model_impl.h"
#include "components/feature_engagement/internal/display_lock_controller_impl.h"
#include "components/feature_engagement/internal/editable_configuration.h"
#include "components/feature_engagement/internal/event_model_impl.h"
#include "components/feature_engagement/internal/feature_config_condition_validator.h"
#include "components/feature_engagement/internal/system_time_provider.h"
#include "components/feature_engagement/public/jni_headers/CppWrappedTestTracker_jni.h"

namespace feature_engagement {

WrappingTestTracker::WrappingTestTracker(
    std::unique_ptr<EventModel> event_model,
    std::unique_ptr<AvailabilityModel> availability_model,
    std::unique_ptr<Configuration> configuration,
    std::unique_ptr<DisplayLockController> display_lock_controller,
    std::unique_ptr<ConditionValidator> condition_validator,
    std::unique_ptr<TimeProvider> time_provider)
    : TrackerImpl(std::move(event_model),
                  std::move(availability_model),
                  std::move(configuration),
                  std::make_unique<DisplayLockControllerImpl>(),
                  std::move(condition_validator),
                  std::move(time_provider)) {}

WrappingTestTracker::~WrappingTestTracker() {}

void WrappingTestTracker::InjectTracker(
    const base::android::JavaRef<jobject>& jtracker) {
  java_tracker_ = jtracker;
}

void WrappingTestTracker::NotifyEvent(const std::string& event) {
  if (!java_tracker_) {
    TrackerImpl::NotifyEvent(event);
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> jevent(
      base::android::ConvertUTF8ToJavaString(env, event.c_str()));
  Java_CppWrappedTestTracker_notifyEvent(base::android::AttachCurrentThread(),
                                         java_tracker_, jevent);
}

bool WrappingTestTracker::ShouldTriggerHelpUI(const base::Feature& feature) {
  if (!java_tracker_)
    return TrackerImpl::ShouldTriggerHelpUI(feature);

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> jfeature(
      base::android::ConvertUTF8ToJavaString(env, feature.name));
  return Java_CppWrappedTestTracker_shouldTriggerHelpUI(
      base::android::AttachCurrentThread(), java_tracker_, jfeature);
}

bool WrappingTestTracker::WouldTriggerHelpUI(
    const base::Feature& feature) const {
  if (!java_tracker_)
    return TrackerImpl::WouldTriggerHelpUI(feature);

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> jfeature(
      base::android::ConvertUTF8ToJavaString(env, feature.name));
  return Java_CppWrappedTestTracker_wouldTriggerHelpUI(
      base::android::AttachCurrentThread(), java_tracker_, jfeature);
}

bool WrappingTestTracker::HasEverTriggered(const base::Feature& feature,
                                           bool from_window) const {
  if (!java_tracker_)
    return TrackerImpl::HasEverTriggered(feature, from_window);

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> jfeature(
      base::android::ConvertUTF8ToJavaString(env, feature.name));
  return Java_CppWrappedTestTracker_hasEverTriggered(
      base::android::AttachCurrentThread(), java_tracker_, jfeature,
      from_window);
}

Tracker::TriggerState WrappingTestTracker::GetTriggerState(
    const base::Feature& feature) const {
  if (!java_tracker_)
    return TrackerImpl::GetTriggerState(feature);

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> jfeature(
      base::android::ConvertUTF8ToJavaString(env, feature.name));
  return static_cast<Tracker::TriggerState>(
      Java_CppWrappedTestTracker_getTriggerState(
          base::android::AttachCurrentThread(), java_tracker_, jfeature));
}

void WrappingTestTracker::Dismissed(const base::Feature& feature) {
  if (!java_tracker_) {
    TrackerImpl::Dismissed(feature);
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> jfeature(
      base::android::ConvertUTF8ToJavaString(env, feature.name));
  Java_CppWrappedTestTracker_dismissed(base::android::AttachCurrentThread(),
                                       java_tracker_, jfeature);
}

std::unique_ptr<DisplayLockHandle> WrappingTestTracker::AcquireDisplayLock() {
  return TrackerImpl::AcquireDisplayLock();
}

bool WrappingTestTracker::IsInitialized() const {
  if (!java_tracker_)
    return TrackerImpl::IsInitialized();

  return Java_CppWrappedTestTracker_isInitialized(
      base::android::AttachCurrentThread(), java_tracker_);
}

void WrappingTestTracker::AddOnInitializedCallback(
    OnInitializedCallback callback) {
  TrackerImpl::AddOnInitializedCallback(std::move(callback));
}

}  // namespace feature_engagement
