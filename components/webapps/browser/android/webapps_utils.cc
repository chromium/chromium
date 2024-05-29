// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/webapps_utils.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/webapps/browser/android/webapk/webapk_types.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/webapps/browser/android/webapps_jni_headers/WebappsUtils_jni.h"

namespace webapps {

namespace {

// Returns whether a URL in the Web Manifest is WebAPK compatible.
bool IsUrlWebApkCompatible(const GURL& url) {
  // WebAPK web manifests are stored on the Chrome WebAPK server. Do not
  // generate WebAPKs for Web Manifests with URLs with a user name or password
  // in order to avoid storing user names and passwords on the WebAPK server.
  return !url.has_username() && !url.has_password();
}

}  // namespace

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

// static
bool WebappsUtils::AreWebManifestUrlsWebApkCompatible(
    const blink::mojom::Manifest& manifest) {
  for (const auto& icon : manifest.icons) {
    if (!IsUrlWebApkCompatible(icon.src))
      return false;
  }

  // Do not check "related_applications" URLs because they are not used by
  // WebAPKs.
  return IsUrlWebApkCompatible(manifest.start_url) &&
         IsUrlWebApkCompatible(manifest.scope);
}

// static
void WebappsUtils::ShowWebApkInstallResultToast(
    webapps::WebApkInstallResult result) {
  Java_WebappsUtils_showWebApkInstallResultToast(
      base::android::AttachCurrentThread(), (int)result);
}

}  // namespace webapps
