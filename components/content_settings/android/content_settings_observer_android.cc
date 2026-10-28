// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/android/content_settings_observer_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/permissions/permissions_client.h"
#include "content/public/browser/android/browser_context_handle.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/content_settings/android/content_settings_jni_headers/ContentSettingsObserver_jni.h"

namespace content_settings {

using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;

AndroidObserver::AndroidObserver(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj,
    const JavaRef<jobject>& jbrowser_context_handle)
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
  Java_ContentSettingsObserver_onContentSettingChanged(
      base::android::AttachCurrentThread(), jobject_,
      primary_pattern.ToString(), secondary_pattern.ToString(),
      static_cast<int>(content_type_set.GetTypeOrDefault()));
}

static jni_zero::JniUniquePtr<AndroidObserver> JNI_ContentSettingsObserver_Init(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj,
    const base::android::JavaRef<jobject>& jbrowser_context_handle) {
  return jni_zero::MakeUnique<AndroidObserver>(env, obj,
                                               jbrowser_context_handle);
}

}  // namespace content_settings

DEFINE_JNI(ContentSettingsObserver)
