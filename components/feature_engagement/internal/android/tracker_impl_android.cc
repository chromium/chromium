// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/android/tracker_impl_android.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feature_engagement/public/tracker.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/feature_engagement/internal/jni_headers/TrackerImpl_jni.h"

namespace feature_engagement {

namespace {

const char kTrackerImplAndroidKey[] = "tracker_impl_android";

// Create mapping from feature name to base::Feature.
TrackerImplAndroid::FeatureMap CreateMapFromNameToFeature(
    FeatureVector features) {
  TrackerImplAndroid::FeatureMap feature_map;
  for (auto it = features.begin(); it != features.end(); ++it) {
    feature_map[(*it)->name] = *it;
  }
  return feature_map;
}

TrackerImplAndroid* FromTracker(Tracker* tracker) {
  TrackerImplAndroid* impl_android = static_cast<TrackerImplAndroid*>(
      tracker->GetUserData(kTrackerImplAndroidKey));
  if (!impl_android) {
    impl_android = new TrackerImplAndroid(tracker, GetAllFeatures());
    tracker->SetUserData(kTrackerImplAndroidKey,
                         base::WrapUnique(impl_android));
  }
  return impl_android;
}

}  // namespace

// static
TrackerImplAndroid* TrackerImplAndroid::FromJavaObject(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj) {
  return reinterpret_cast<TrackerImplAndroid*>(
      Java_TrackerImpl_getNativePtr(env, jobj));
}

// This function is declared in //components/feature_engagement/public/tracker.h
// and should be linked in to any binary using Tracker::GetJavaObject.
// static
base::android::ScopedJavaLocalRef<jobject> Tracker::GetJavaObject(
    Tracker* feature_engagement) {
  return FromTracker(feature_engagement)->GetJavaObject();
}

TrackerImplAndroid::TrackerImplAndroid(Tracker* tracker, FeatureVector features)
    : features_(CreateMapFromNameToFeature(features)), tracker_(tracker) {
  JNIEnv* env = base::android::AttachCurrentThread();

  java_obj_.Reset(
      env,
      Java_TrackerImpl_create(env, reinterpret_cast<intptr_t>(this)).obj());
}

TrackerImplAndroid::~TrackerImplAndroid() {
  Java_TrackerImpl_clearNativePtr(base::android::AttachCurrentThread(),
                                  java_obj_);
}

base::android::ScopedJavaLocalRef<jobject> TrackerImplAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_obj_);
}

void TrackerImplAndroid::NotifyEvent(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj,
    const base::android::JavaParamRef<jstring>& jevent) {
  std::string event = base::android::ConvertJavaStringToUTF8(env, jevent);
  tracker_->NotifyEvent(event);
}

bool TrackerImplAndroid::ShouldTriggerHelpUI(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj,
    const base::android::JavaParamRef<jstring>& jfeature) {
  std::string feature = base::android::ConvertJavaStringToUTF8(env, jfeature);
  DCHECK(features_.find(feature) != features_.end());

  return tracker_->ShouldTriggerHelpUI(*features_[feature]);
}

base::android::ScopedJavaLocalRef<jobject>
TrackerImplAndroid::ShouldTriggerHelpUIWithSnooze(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj,
    const base::android::JavaParamRef<jstring>& jfeature) {
  std::string feature = base::android::ConvertJavaStringToUTF8(env, jfeature);
  DCHECK(features_.find(feature) != features_.end());

  Tracker::TriggerDetails triggerDetails =
      tracker_->ShouldTriggerHelpUIWithSnooze(*features_[feature]);
  return Java_TrackerImpl_createTriggerDetails(
      env, triggerDetails.ShouldShowIph(), triggerDetails.ShouldShowSnooze());
}

bool TrackerImplAndroid::WouldTriggerHelpUI(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj,
    const base::android::JavaParamRef<jstring>& jfeature) {
  std::string feature = base::android::ConvertJavaStringToUTF8(env, jfeature);
  DCHECK(features_.find(feature) != features_.end());

  return tracker_->WouldTriggerHelpUI(*features_[feature]);
}

bool TrackerImplAndroid::HasEverTriggered(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj,
    const base::android::JavaParamRef<jstring>& jfeature,
    const jboolean j_from_window) {
  std::string feature = base::android::ConvertJavaStringToUTF8(env, jfeature);
  DCHECK(features_.find(feature) != features_.end());

  return tracker_->HasEverTriggered(*features_[feature], j_from_window);
}

jint TrackerImplAndroid::GetTriggerState(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj,
    const base::android::JavaParamRef<jstring>& jfeature) {
  std::string feature = base::android::ConvertJavaStringToUTF8(env, jfeature);
  DCHECK(features_.find(feature) != features_.end());

  return static_cast<int>(tracker_->GetTriggerState(*features_[feature]));
}

void TrackerImplAndroid::Dismissed(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj,
    const base::android::JavaParamRef<jstring>& jfeature) {
  std::string feature = base::android::ConvertJavaStringToUTF8(env, jfeature);
  DCHECK(features_.find(feature) != features_.end());

  tracker_->Dismissed(*features_[feature]);
}

void TrackerImplAndroid::DismissedWithSnooze(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj,
    const base::android::JavaParamRef<jstring>& jfeature,
    const jint snooze_action) {
  std::string feature = base::android::ConvertJavaStringToUTF8(env, jfeature);
  DCHECK(features_.find(feature) != features_.end());

  tracker_->DismissedWithSnooze(
      *features_[feature],
      std::make_optional(static_cast<Tracker::SnoozeAction>(snooze_action)));
}

base::android::ScopedJavaLocalRef<jobject>
TrackerImplAndroid::AcquireDisplayLock(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj) {
  std::unique_ptr<DisplayLockHandle> lock_handle =
      tracker_->AcquireDisplayLock();
  if (!lock_handle)
    return nullptr;

  auto lock_handle_android =
      std::make_unique<DisplayLockHandleAndroid>(std::move(lock_handle));

  // Intentionally release ownership to Java.
  // Callers are required to invoke DisplayLockHandleAndroid#release().
  return lock_handle_android.release()->GetJavaObject();
}

void TrackerImplAndroid::SetPriorityNotification(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj,
    const base::android::JavaParamRef<jstring>& jfeature) {
  std::string feature = base::android::ConvertJavaStringToUTF8(env, jfeature);
  DCHECK(features_.find(feature) != features_.end());

  return tracker_->SetPriorityNotification(*features_[feature]);
}

base::android::ScopedJavaLocalRef<jstring>
TrackerImplAndroid::GetPendingPriorityNotification(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj) {
  auto notification = tracker_->GetPendingPriorityNotification();
  std::string pending_notification_string =
      notification.value_or(std::string());
  return base::android::ConvertUTF8ToJavaString(env,
                                                pending_notification_string);
}

void TrackerImplAndroid::RegisterPriorityNotificationHandler(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj,
    const base::android::JavaParamRef<jstring>& jfeature,
    const base::android::JavaRef<jobject>& jrunnable) {
  std::string feature = base::android::ConvertJavaStringToUTF8(env, jfeature);
  DCHECK(features_.find(feature) != features_.end());

  return tracker_->RegisterPriorityNotificationHandler(
      *features_[feature],
      base::BindOnce(&base::android::RunRunnableAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(jrunnable)));
}

void TrackerImplAndroid::UnregisterPriorityNotificationHandler(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj,
    const base::android::JavaParamRef<jstring>& jfeature) {
  std::string feature = base::android::ConvertJavaStringToUTF8(env, jfeature);
  DCHECK(features_.find(feature) != features_.end());

  return tracker_->UnregisterPriorityNotificationHandler(*features_[feature]);
}

bool TrackerImplAndroid::IsInitialized(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj) {
  return tracker_->IsInitialized();
}

void TrackerImplAndroid::AddOnInitializedCallback(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj,
    const base::android::JavaParamRef<jobject>& j_callback_obj) {
  tracker_->AddOnInitializedCallback(base::BindOnce(
      &base::android::RunBooleanCallbackAndroid,
      base::android::ScopedJavaGlobalRef<jobject>(j_callback_obj)));
}

DisplayLockHandleAndroid::DisplayLockHandleAndroid(
    std::unique_ptr<DisplayLockHandle> display_lock_handle)
    : display_lock_handle_(std::move(display_lock_handle)) {
  java_obj_.Reset(
      base::android::AttachCurrentThread(),
      Java_DisplayLockHandleAndroid_create(base::android::AttachCurrentThread(),
                                           reinterpret_cast<intptr_t>(this))
          .obj());
}

DisplayLockHandleAndroid::~DisplayLockHandleAndroid() {
  Java_DisplayLockHandleAndroid_clearNativePtr(
      base::android::AttachCurrentThread(), java_obj_);
}

base::android::ScopedJavaLocalRef<jobject>
DisplayLockHandleAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_obj_);
}

void DisplayLockHandleAndroid::Release(JNIEnv* env) {
  delete this;
}

}  // namespace feature_engagement
