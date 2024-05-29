// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/android/content_settings_observer_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/permissions/permissions_client.h"
#include "content/public/browser/android/browser_context_handle.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/content_settings/android/content_settings_jni_headers/ContentSettingsObserver_jni.h"

namespace content_settings {

using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;

AndroidObserver::AndroidObserver(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jbrowser_context_handle)
    : jobject_(obj) {
  content::BrowserContext* browser_context =
      content::BrowserContextFromJavaHandle(jbrowser_context_handle);

  auto* host_content_settings_map =
      permissions::PermissionsClient::Get()->GetSettingsMap(browser_context);
  content_settings_observation_.Observe(host_content_settings_map);
}

AndroidObserver::~AndroidObserver() = default;

void AndroidObserver::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_ContentSettingsObserver_onContentSettingChanged(
      env, jobject_, ConvertUTF8ToJavaString(env, primary_pattern.ToString()),
      ConvertUTF8ToJavaString(env, secondary_pattern.ToString()),
      static_cast<int>(content_type_set.GetTypeOrDefault()));
}

void AndroidObserver::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

static jlong JNI_ContentSettingsObserver_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& jbrowser_context_handle) {
  return reinterpret_cast<intptr_t>(
      new AndroidObserver(env, obj, jbrowser_context_handle));
}

}  // namespace content_settings
