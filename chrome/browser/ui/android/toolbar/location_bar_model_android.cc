// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/toolbar/location_bar_model_android.h"

#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/LocationBarModel_jni.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/omnibox/browser/location_bar_model_impl.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "ui/base/device_form_factor.h"

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

ScopedJavaLocalRef<jstring> LocationBarModelAndroid::GetDisplaySearchTerms(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  base::string16 result;
  if (!location_bar_model_->GetDisplaySearchTerms(&result))
    return nullptr;

  return base::android::ConvertUTF16ToJavaString(env, result);
}

jint LocationBarModelAndroid::GetPageClassification(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    bool is_focused_from_fakebox) {
  // On phones, the omnibox is not initially shown on the NTP.  In this case,
  // treat the fakebox like the omnibox.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE)
    is_focused_from_fakebox = false;

  // On tablets, the user can choose to focus either the fakebox or the
  // omnibox.  Chrome distinguishes between the two in order to apply URL
  // demotion when the user focuses the fakebox (which looks more like a
  // search box) but not when they focus the omnibox (which looks more
  // like a URL bar).
  OmniboxFocusSource source = is_focused_from_fakebox
                                  ? OmniboxFocusSource::FAKEBOX
                                  : OmniboxFocusSource::OMNIBOX;

  // TODO: Android does not save the homepage to the native pref, so we will
  // never get the HOME_PAGE classification. Fix this by overriding IsHomePage.
  return location_bar_model_->GetPageClassification(source);
}

content::WebContents* LocationBarModelAndroid::GetActiveWebContents() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jweb_contents =
      Java_LocationBarModel_getActiveWebContents(env, java_object_);
  return content::WebContents::FromJavaWebContents(jweb_contents);
}

bool LocationBarModelAndroid::IsInstantNTP() const {
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
