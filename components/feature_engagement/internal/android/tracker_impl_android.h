// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_ANDROID_TRACKER_IMPL_ANDROID_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_ANDROID_TRACKER_IMPL_ANDROID_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "components/feature_engagement/internal/tracker_impl.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feature_engagement/public/tracker.h"

namespace feature_engagement {

// JNI bridge between DisplayLockHandleAndroid in Java and C++.
// This class owns the underlying DisplayLockHandle acquired from the backing
// Tracker. Ownership of this class is released to Java, which means Java owners
// MUST call release() before removing their reference to this object.
// This class must be in this header file, since it is used by the generated
// JNI code.
class DisplayLockHandleAndroid {
 public:
  DisplayLockHandleAndroid(
      std::unique_ptr<DisplayLockHandle> display_lock_handle);

  DisplayLockHandleAndroid(const DisplayLockHandleAndroid&) = delete;
  DisplayLockHandleAndroid& operator=(const DisplayLockHandleAndroid&) = delete;

  ~DisplayLockHandleAndroid();

  // Returns the Java-side of this JNI bridge.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // Deletes |this|.
  void Release(JNIEnv* env);

 private:
  // The DisplayLockHandle that this JNI bridge owns.
  std::unique_ptr<DisplayLockHandle> display_lock_handle_;

  // The Java-side of this JNI bridge.
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
};

// JNI bridge between TrackerImpl in Java and C++. See the
// public API of Tracker for documentation for all methods.
class TrackerImplAndroid : public base::SupportsUserData::Data {
 public:
  using FeatureMap = std::unordered_map<std::string, const base::Feature*>;
  static TrackerImplAndroid* FromJavaObject(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jobj);

  TrackerImplAndroid(Tracker* tracker, FeatureVector features);

  TrackerImplAndroid(const TrackerImplAndroid&) = delete;
  TrackerImplAndroid& operator=(const TrackerImplAndroid&) = delete;

  ~TrackerImplAndroid() override;

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  Tracker* tracker() { return tracker_; }

  // Tracker JNI bridge implementation.
  virtual void NotifyEvent(JNIEnv* env,
                           const base::android::JavaRef<jobject>& jobj,
                           const base::android::JavaParamRef<jstring>& jevent);
  virtual bool ShouldTriggerHelpUI(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jobj,
      const base::android::JavaParamRef<jstring>& jfeature);
  virtual base::android::ScopedJavaLocalRef<jobject>
  ShouldTriggerHelpUIWithSnooze(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jobj,
      const base::android::JavaParamRef<jstring>& jfeature);
  virtual bool WouldTriggerHelpUI(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jobj,
      const base::android::JavaParamRef<jstring>& jfeature);
  virtual bool HasEverTriggered(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jobj,
      const base::android::JavaParamRef<jstring>& jfeature,
      const jboolean j_from_window);
  virtual jint GetTriggerState(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jobj,
      const base::android::JavaParamRef<jstring>& jfeature);
  virtual void Dismissed(JNIEnv* env,
                         const base::android::JavaRef<jobject>& jobj,
                         const base::android::JavaParamRef<jstring>& jfeature);
  virtual void DismissedWithSnooze(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jobj,
      const base::android::JavaParamRef<jstring>& jfeature,
      const jint snooze_action);
  virtual base::android::ScopedJavaLocalRef<jobject> AcquireDisplayLock(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jobj);
  virtual void SetPriorityNotification(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jobj,
      const base::android::JavaParamRef<jstring>& jfeature);
  virtual base::android::ScopedJavaLocalRef<jstring>
  GetPendingPriorityNotification(JNIEnv* env,
                                 const base::android::JavaRef<jobject>& jobj);
  virtual void RegisterPriorityNotificationHandler(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jobj,
      const base::android::JavaParamRef<jstring>& jfeature,
      const base::android::JavaRef<jobject>& jcallback);
  virtual void UnregisterPriorityNotificationHandler(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jobj,
      const base::android::JavaParamRef<jstring>& jfeature);
  virtual bool IsInitialized(JNIEnv* env,
                             const base::android::JavaRef<jobject>& jobj);
  virtual void AddOnInitializedCallback(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jobj,
      const base::android::JavaParamRef<jobject>& j_callback_obj);

 private:
  // A map from the feature name to the base::Feature, to ensure that the Java
  // version of the API can use the string name. If base::Feature becomes a Java
  // class as well, we should remove this mapping.
  FeatureMap features_;

  // The Tracker this is a JNI bridge for.
  raw_ptr<Tracker> tracker_;

  // The Java-side of this JNI bridge.
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_ANDROID_TRACKER_IMPL_ANDROID_H_
