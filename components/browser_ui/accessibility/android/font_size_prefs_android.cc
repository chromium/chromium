// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_ui/accessibility/android/font_size_prefs_android.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/android/browser_context_handle.h"
#include "content/public/browser/browser_context.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/browser_ui/accessibility/android/accessibility_jni_headers/FontSizePrefs_jni.h"

namespace browser_ui {

using base::android::JavaParamRef;
using base::android::JavaRef;

namespace prefs {
const char kWebKitFontScaleFactor[] = "webkit.webprefs.font_scale_factor";
const char kWebKitForceEnableZoom[] = "webkit.webprefs.force_enable_zoom";
}  // namespace prefs

FontSizePrefsAndroid::FontSizePrefsAndroid(
    JNIEnv* env,
    jobject obj,
    const JavaParamRef<jobject>& jbrowser_context_handle)
    : pref_service_(user_prefs::UserPrefs::Get(
          content::BrowserContextFromJavaHandle(jbrowser_context_handle))) {
  java_ref_.Reset(env, obj);
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service_);
  pref_change_registrar_->Add(
      prefs::kWebKitFontScaleFactor,
      base::BindRepeating(&FontSizePrefsAndroid::OnFontScaleFactorChanged,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kWebKitForceEnableZoom,
      base::BindRepeating(&FontSizePrefsAndroid::OnForceEnableZoomChanged,
                          base::Unretained(this)));
}

FontSizePrefsAndroid::~FontSizePrefsAndroid() = default;

void FontSizePrefsAndroid::SetFontScaleFactor(JNIEnv* env,
                                              const JavaRef<jobject>& obj,
                                              jfloat font_size) {
  pref_service_->SetDouble(prefs::kWebKitFontScaleFactor,
                           static_cast<double>(font_size));
}

float FontSizePrefsAndroid::GetFontScaleFactor(JNIEnv* env,
                                               const JavaRef<jobject>& obj) {
  return pref_service_->GetDouble(prefs::kWebKitFontScaleFactor);
}

void FontSizePrefsAndroid::SetForceEnableZoom(JNIEnv* env,
                                              const JavaRef<jobject>& obj,
                                              jboolean enabled) {
  pref_service_->SetBoolean(prefs::kWebKitForceEnableZoom, enabled);
}

bool FontSizePrefsAndroid::GetForceEnableZoom(JNIEnv* env,
                                              const JavaRef<jobject>& obj) {
  return pref_service_->GetBoolean(prefs::kWebKitForceEnableZoom);
}

void FontSizePrefsAndroid::Destroy(JNIEnv* env) {
  delete this;
}

jlong JNI_FontSizePrefs_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& jbrowser_context_handle) {
  FontSizePrefsAndroid* font_size_prefs_android =
      new FontSizePrefsAndroid(env, obj, jbrowser_context_handle);
  return reinterpret_cast<intptr_t>(font_size_prefs_android);
}

void FontSizePrefsAndroid::OnFontScaleFactorChanged() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  float factor = GetFontScaleFactor(env, java_ref_);
  Java_FontSizePrefs_onFontScaleFactorChanged(env, java_ref_, factor);
}

void FontSizePrefsAndroid::OnForceEnableZoomChanged() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  bool enabled = GetForceEnableZoom(env, java_ref_);
  Java_FontSizePrefs_onForceEnableZoomChanged(env, java_ref_, enabled);
}

}  // namespace browser_ui
