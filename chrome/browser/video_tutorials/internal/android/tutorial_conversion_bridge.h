// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_ANDROID_TUTORIAL_CONVERSION_BRIDGE_H_
#define CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_ANDROID_TUTORIAL_CONVERSION_BRIDGE_H_

#include <vector>

#include "base/android/jni_android.h"
#include "base/optional.h"
#include "chrome/browser/video_tutorials/tutorial.h"

using base::android::ScopedJavaLocalRef;

namespace video_tutorials {

// Helper class providing video tutorial conversion utility methods between C++
// and Java.
class TutorialConversionBridge {
 public:
  static ScopedJavaLocalRef<jobject> CreateJavaTutorials(
      JNIEnv* env,
      const std::vector<Tutorial>& tutorials);

  static ScopedJavaLocalRef<jobject> CreateJavaTutorial(
      JNIEnv* env,
      base::Optional<Tutorial> tutorial);
};

}  // namespace video_tutorials

#endif  // CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_ANDROID_TUTORIAL_CONVERSION_BRIDGE_H_
