// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_ANDROID_VIDEO_TUTORIAL_SERVICE_BRIDGE_H_
#define CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_ANDROID_VIDEO_TUTORIAL_SERVICE_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "chrome/browser/video_tutorials/video_tutorial_service.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace video_tutorials {

// Helper class responsible for bridging the VideoTutorialService between C++
// and Java.
class VideoTutorialServiceBridge : public base::SupportsUserData::Data {
 public:
  // Returns a Java VideoTutorialServiceBridge for |video_tutorial_service|.
  // There will be only one bridge per VideoTutorialServiceBridge.
  static ScopedJavaLocalRef<jobject> GetBridgeForVideoTutorialService(
      VideoTutorialService* video_tutorial_service);

  explicit VideoTutorialServiceBridge(
      VideoTutorialService* video_tutorial_service);

  VideoTutorialServiceBridge(const VideoTutorialServiceBridge&) = delete;
  VideoTutorialServiceBridge& operator=(const VideoTutorialServiceBridge&) =
      delete;

  ~VideoTutorialServiceBridge() override;

  // Methods called from Java via JNI.
  void GetTutorials(JNIEnv* env,
                    const JavaParamRef<jobject>& jcaller,
                    const JavaParamRef<jobject>& jcallback);
  void GetTutorial(JNIEnv* env,
                   const JavaParamRef<jobject>& jcaller,
                   jint j_feature,
                   const JavaParamRef<jobject>& jcallback);
  ScopedJavaLocalRef<jobjectArray> GetSupportedLanguages(
      JNIEnv* env,
      const JavaParamRef<jobject>& jcaller);
  ScopedJavaLocalRef<jobjectArray> GetAvailableLanguagesForTutorial(
      JNIEnv* env,
      const JavaParamRef<jobject>& jcaller,
      jint j_feature);
  ScopedJavaLocalRef<jstring> GetPreferredLocale(
      JNIEnv* env,
      const JavaParamRef<jobject>& jcaller);
  void SetPreferredLocale(JNIEnv* env,
                          const JavaParamRef<jobject>& jcaller,
                          jstring j_locale);

 private:
  // A reference to the Java counterpart of this class.  See
  // VideoTutorialServiceBridge.java.
  ScopedJavaGlobalRef<jobject> java_obj_;

  // Not owned.
  raw_ptr<VideoTutorialService> video_tutorial_service_;
};

}  // namespace video_tutorials

#endif  // CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_ANDROID_VIDEO_TUTORIAL_SERVICE_BRIDGE_H_
