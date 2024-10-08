// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/android/wrapping_test_tracker.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/android/jni_string.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/feature_engagement/public/jni_headers/CppWrappedTestTracker_jni.h"

namespace feature_engagement {

WrappingTestTracker::WrappingTestTracker(
    const base::android::JavaRef<jobject>& jtracker)
    : java_tracker_(jtracker) {}
WrappingTestTracker::~WrappingTestTracker() = default;

void WrappingTestTracker::NotifyEvent(const std::string& event) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> jevent(
      base::android::ConvertUTF8ToJavaString(env, event.c_str()));
  Java_CppWrappedTestTracker_notifyEvent(base::android::AttachCurrentThread(),
                                         java_tracker_, jevent);
}

bool WrappingTestTracker::ShouldTriggerHelpUI(const base::Feature& feature) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> jfeature(
      base::android::ConvertUTF8ToJavaString(env, feature.name));
  return Java_CppWrappedTestTracker_shouldTriggerHelpUI(
      base::android::AttachCurrentThread(), java_tracker_, jfeature);
}

Tracker::TriggerDetails WrappingTestTracker::ShouldTriggerHelpUIWithSnooze(
    const base::Feature& feature) {
  return Tracker::TriggerDetails(false, false);
}

bool WrappingTestTracker::WouldTriggerHelpUI(
    const base::Feature& feature) const {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> jfeature(
      base::android::ConvertUTF8ToJavaString(env, feature.name));
  return Java_CppWrappedTestTracker_wouldTriggerHelpUI(
      base::android::AttachCurrentThread(), java_tracker_, jfeature);
}

bool WrappingTestTracker::HasEverTriggered(const base::Feature& feature,
                                           bool from_window) const {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> jfeature(
      base::android::ConvertUTF8ToJavaString(env, feature.name));
  return Java_CppWrappedTestTracker_hasEverTriggered(
      base::android::AttachCurrentThread(), java_tracker_, jfeature,
      from_window);
}

Tracker::TriggerState WrappingTestTracker::GetTriggerState(
    const base::Feature& feature) const {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> jfeature(
      base::android::ConvertUTF8ToJavaString(env, feature.name));
  return static_cast<Tracker::TriggerState>(
      Java_CppWrappedTestTracker_getTriggerState(
          base::android::AttachCurrentThread(), java_tracker_, jfeature));
}

void WrappingTestTracker::Dismissed(const base::Feature& feature) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> jfeature(
      base::android::ConvertUTF8ToJavaString(env, feature.name));
  Java_CppWrappedTestTracker_dismissed(base::android::AttachCurrentThread(),
                                       java_tracker_, jfeature);
}

void WrappingTestTracker::DismissedWithSnooze(
    const base::Feature& feature,
    std::optional<SnoozeAction> snooze_action) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> jfeature(
      base::android::ConvertUTF8ToJavaString(env, feature.name));
  Java_CppWrappedTestTracker_dismissedWithSnooze(
      base::android::AttachCurrentThread(), java_tracker_, jfeature,
      static_cast<int>(snooze_action.value()));
}

std::unique_ptr<DisplayLockHandle> WrappingTestTracker::AcquireDisplayLock() {
  return nullptr;
}

void WrappingTestTracker::SetPriorityNotification(
    const base::Feature& feature) {}

std::optional<std::string>
WrappingTestTracker::GetPendingPriorityNotification() {
  return std::nullopt;
}

void WrappingTestTracker::RegisterPriorityNotificationHandler(
    const base::Feature& feature,
    base::OnceClosure callback) {}

void WrappingTestTracker::UnregisterPriorityNotificationHandler(
    const base::Feature& feature) {}

bool WrappingTestTracker::IsInitialized() const {
  return Java_CppWrappedTestTracker_isInitialized(
      base::android::AttachCurrentThread(), java_tracker_);
}

void WrappingTestTracker::AddOnInitializedCallback(
    OnInitializedCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), IsInitialized()));
}

const Configuration* WrappingTestTracker::GetConfigurationForTesting() const {
  NOTIMPLEMENTED();
  return nullptr;
}

void WrappingTestTracker::SetClockForTesting(const base::Clock& clock,
                                             base::Time initial_time) {
  NOTIMPLEMENTED();
}

}  // namespace feature_engagement
