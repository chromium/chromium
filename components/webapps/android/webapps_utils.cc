// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/android/webapps_utils.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/webapps/android/webapps_jni_headers/WebappsUtils_jni.h"
#include "url/gurl.h"

namespace webapps {

// static
bool WebappsUtils::IsWebApkInstalled(content::BrowserContext* browser_context,
                                     const GURL& url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> java_url =
      base::android::ConvertUTF8ToJavaString(env, url.spec());
  base::android::ScopedJavaLocalRef<jstring> java_webapk_package_name =
      Java_WebappsUtils_queryFirstWebApkPackage(env, java_url);

  std::string webapk_package_name;
  if (java_webapk_package_name.obj()) {
    webapk_package_name =
        base::android::ConvertJavaStringToUTF8(env, java_webapk_package_name);
  }
  return !webapk_package_name.empty();
}

}  // namespace webapps
