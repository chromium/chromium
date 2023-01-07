// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_ANDROID_TUTORIAL_CONVERSION_BRIDGE_H_
#define CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_ANDROID_TUTORIAL_CONVERSION_BRIDGE_H_

#include <vector>

#include "base/android/jni_android.h"
#include "chrome/browser/video_tutorials/tutorial.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
      absl::optional<Tutorial> tutorial);
};

}  // namespace video_tutorials

#endif  // CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_ANDROID_TUTORIAL_CONVERSION_BRIDGE_H_
