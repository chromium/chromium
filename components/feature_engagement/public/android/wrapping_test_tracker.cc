// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/android/wrapping_test_tracker.h"

#include <memory>
#include <utility>

#include "base/android/jni_string.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/feature_engagement/public/jni_headers/CppWrappedTestTracker_jni.h"

namespace feature_engagement {

WrappingTestTracker::WrappingTestTracker(
    const base::android::JavaRef<jobject>& jtracker)
    : java_tracker_(jtracker) {}
WrappingTestTracker::~WrappingTestTracker() {}

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

std::unique_ptr<DisplayLockHandle> WrappingTestTracker::AcquireDisplayLock() {
  return nullptr;
}

bool WrappingTestTracker::IsInitialized() const {
  return Java_CppWrappedTestTracker_isInitialized(
      base::android::AttachCurrentThread(), java_tracker_);
}

void WrappingTestTracker::AddOnInitializedCallback(
    OnInitializedCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), IsInitialized()));
}

}  // namespace feature_engagement
