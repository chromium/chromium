// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_ANDROID_LARGE_ICON_BRIDGE_H_
#define COMPONENTS_FAVICON_ANDROID_LARGE_ICON_BRIDGE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/task/cancelable_task_tracker.h"

namespace favicon {

// The C++ counterpart to Java's LargeIconBridge. Together these classes expose
// LargeIconService to Java.
class LargeIconBridge {
 public:
  LargeIconBridge();
  LargeIconBridge(const LargeIconBridge& bridge) = delete;
  LargeIconBridge& operator=(const LargeIconBridge& bridge) = delete;

  void Destroy(JNIEnv* env);
  jboolean GetLargeIconForURL(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_browser_context,
      const base::android::JavaParamRef<jobject>& j_page_url,
      jint min_source_size_px,
      const base::android::JavaParamRef<jobject>& j_callback);

 private:
  virtual ~LargeIconBridge();

  base::CancelableTaskTracker cancelable_task_tracker_;
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_ANDROID_LARGE_ICON_BRIDGE_H_
