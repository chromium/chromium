// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_ANDROID_LARGE_ICON_BRIDGE_H_
#define COMPONENTS_FAVICON_ANDROID_LARGE_ICON_BRIDGE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon_base/favicon_types.h"

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
      jint desired_source_size_px,
      const base::android::JavaParamRef<jobject>& j_callback);
  void GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_browser_context,
      const base::android::JavaParamRef<jobject>& j_page_url,
      jboolean should_trim_page_url_path,
      jint j_network_annotation_hash_code,
      const base::android::JavaParamRef<jobject>& j_callback);
  void TouchIconFromGoogleServer(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_browser_context,
      const base::android::JavaParamRef<jobject>& j_page_url);

 private:
  virtual ~LargeIconBridge();

  void OnGoogleFaviconServerResponse(
      const base::android::JavaRef<jobject>& j_callback,
      favicon_base::GoogleFaviconServerRequestStatus status) const;

  // TODO(crbug.com/41485636): Remove this when LargeIconService no longer
  // relies
  //                          on CancelableTaskTracker.
  base::CancelableTaskTracker cancelable_task_tracker_;

  base::WeakPtrFactory<LargeIconBridge> weak_factory_{this};
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_ANDROID_LARGE_ICON_BRIDGE_H_
