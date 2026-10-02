// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/webapps_utils.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "components/webapps/browser/android/webapk/webapk_types.h"
#include "components/webapps/browser/features.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"
#include "url/origin.h"

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

// Returns whether a navigation entry `url` belongs to the web app's `scope`.
// `scope_origin` must be `url::Origin::Create(scope)`; it is passed in so
// callers checking many URLs against the same scope only compute it once.
bool IsUrlInScope(const GURL& url,
                  const GURL& scope,
                  const url::Origin& scope_origin) {
  if (!url.is_valid() || !scope.is_valid()) {
    return false;
  }
  if (!scope_origin.IsSameOriginWith(url)) {
    return false;
  }
  if (!scope.has_path() || scope.GetPath() == "/") {
    return true;
  }
  return base::StartsWith(url.GetPath(), scope.GetPath(),
                          base::CompareCase::SENSITIVE);
}

}  // namespace

// static
bool WebappsUtils::IsWebApkInstalled(const GURL& url) {
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

// static
bool WebappsUtils::IsAutoMintedTwaEnabled() {
  if (!base::FeatureList::IsEnabled(webapps::features::kAndroidAutoMintedTWA)) {
    return false;
  }
  return Java_WebappsUtils_isWebAppServiceEnabled(
      base::android::AttachCurrentThread());
}

// static
void WebappsUtils::PrunePreScopeNavigationHistory(
    content::WebContents* web_contents,
    const GURL& scope) {
  if (!web_contents || !scope.is_valid()) {
    return;
  }
  content::NavigationController& navigation_controller =
      web_contents->GetController();
  if (!navigation_controller.CanPruneAllButLastCommitted()) {
    return;
  }

  const int last_committed_index =
      navigation_controller.GetLastCommittedEntryIndex();
  const url::Origin scope_origin = url::Origin::Create(scope);
  int index = last_committed_index;
  while (index >= 0 &&
         IsUrlInScope(navigation_controller.GetEntryAtIndex(index)->GetURL(),
                      scope, scope_origin)) {
    --index;
  }

  if (index < 0 || index == last_committed_index) {
    return;
  }

  const content::NavigationEntry* last_entry_to_prune =
      navigation_controller.GetEntryAtIndex(index);
  bool reached_last_entry_to_prune = false;
  navigation_controller.DeleteNavigationEntries(base::BindRepeating(
      [](const content::NavigationEntry* last_entry_to_prune,
         bool* reached_last_entry_to_prune, content::NavigationEntry* entry) {
        if (*reached_last_entry_to_prune) {
          return false;
        }
        if (entry == last_entry_to_prune) {
          *reached_last_entry_to_prune = true;
        }
        return true;
      },
      last_entry_to_prune, base::Unretained(&reached_last_entry_to_prune)));
}

static void JNI_WebappsUtils_PrunePreScopeNavigationHistory(
    JNIEnv* env,
    content::WebContents* web_contents,
    const GURL& scope) {
  WebappsUtils::PrunePreScopeNavigationHistory(web_contents, scope);
}

DEFINE_JNI(WebappsUtils)

}  // namespace webapps
