// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/toolbar/location_bar_model_android.h"

#include "base/android/jni_string.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/omnibox/browser/location_bar_model_impl.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "ui/base/device_form_factor.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/toolbar/jni_headers/LocationBarModel_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

LocationBarModelAndroid::LocationBarModelAndroid(JNIEnv* env,
                                                 const JavaRef<jobject>& obj)
    : location_bar_model_(
          std::make_unique<LocationBarModelImpl>(this,
                                                 content::kMaxURLDisplayChars)),
      java_object_(obj) {}

LocationBarModelAndroid::~LocationBarModelAndroid() {}

void LocationBarModelAndroid::Destroy(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  delete this;
}

ScopedJavaLocalRef<jstring> LocationBarModelAndroid::GetFormattedFullURL(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return base::android::ConvertUTF16ToJavaString(
      env, location_bar_model_->GetFormattedFullURL());
}

ScopedJavaLocalRef<jstring> LocationBarModelAndroid::GetURLForDisplay(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return base::android::ConvertUTF16ToJavaString(
      env, location_bar_model_->GetURLForDisplay());
}

ScopedJavaLocalRef<jobject>
LocationBarModelAndroid::GetUrlOfVisibleNavigationEntry(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return url::GURLAndroid::FromNativeGURL(env, location_bar_model_->GetURL());
}

jint LocationBarModelAndroid::GetPageClassification(JNIEnv* env,
                                                    bool is_prefetch) const {
  return location_bar_model_->GetPageClassification(is_prefetch);
}

content::WebContents* LocationBarModelAndroid::GetActiveWebContents() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jweb_contents =
      Java_LocationBarModel_getActiveWebContents(env, java_object_);
  return content::WebContents::FromJavaWebContents(jweb_contents);
}

bool LocationBarModelAndroid::IsNewTabPage() const {
  GURL url;
  if (!GetURL(&url))
    return false;

  // Android Chrome has its own Instant NTP page implementation.
  if (url.SchemeIs(chrome::kChromeNativeScheme) &&
      url.host_piece() == chrome::kChromeUINewTabHost) {
    return true;
  }

  return false;
}

// static
jlong JNI_LocationBarModel_Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  return reinterpret_cast<intptr_t>(new LocationBarModelAndroid(env, obj));
}
