// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/android/template_url_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#import "build/branding_buildflags.h"
#include "components/search_engines/template_url.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/android/gurl_android.h"

#if BUILDFLAG(ENABLE_BUILTIN_SEARCH_PROVIDER_ASSETS)
#include "third_party/search_engines_data/search_engines_scaled_resources_map.h"
#endif

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/search_engines/android/jni_headers/TemplateUrl_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

TemplateURL* ToTemplateURL(jlong j_template_url) {
  return reinterpret_cast<TemplateURL*>(j_template_url);
}

ScopedJavaLocalRef<jstring> JNI_TemplateUrl_GetShortName(
    JNIEnv* env,
    jlong template_url_ptr) {
  TemplateURL* template_url = ToTemplateURL(template_url_ptr);
  return base::android::ConvertUTF16ToJavaString(env,
                                                 template_url->short_name());
}

ScopedJavaLocalRef<jstring> JNI_TemplateUrl_GetKeyword(JNIEnv* env,
                                                       jlong template_url_ptr) {
  TemplateURL* template_url = ToTemplateURL(template_url_ptr);
  return base::android::ConvertUTF16ToJavaString(env, template_url->keyword());
}

ScopedJavaLocalRef<jobject> JNI_TemplateUrl_GetFaviconURL(
    JNIEnv* env,
    jlong template_url_ptr) {
  TemplateURL* template_url = ToTemplateURL(template_url_ptr);

  return url::GURLAndroid::FromNativeGURL(env, template_url->favicon_url());
}

jboolean JNI_TemplateUrl_IsPrepopulatedOrDefaultProviderByPolicy(
    JNIEnv* env,
    jlong template_url_ptr) {
  TemplateURL* template_url = ToTemplateURL(template_url_ptr);
  return template_url->prepopulate_id() > 0 ||
         template_url->CreatedByPolicy() ||
         template_url->CreatedByRegulatoryProgram();
}

jlong JNI_TemplateUrl_GetLastVisitedTime(JNIEnv* env, jlong template_url_ptr) {
  TemplateURL* template_url = ToTemplateURL(template_url_ptr);
  return template_url->last_visited().InMillisecondsSinceUnixEpoch();
}

jint JNI_TemplateUrl_GetPrepopulatedId(JNIEnv* env, jlong template_url_ptr) {
  TemplateURL* template_url = ToTemplateURL(template_url_ptr);
  return template_url->prepopulate_id();
}

ScopedJavaLocalRef<jobject> CreateTemplateUrlAndroid(
    JNIEnv* env,
    const TemplateURL* template_url) {
  return Java_TemplateUrl_create(env, reinterpret_cast<intptr_t>(template_url));
}

ScopedJavaLocalRef<jstring> JNI_TemplateUrl_GetURL(JNIEnv* env,
                                                   jlong template_url_ptr) {
  TemplateURL* template_url = ToTemplateURL(template_url_ptr);
  return base::android::ConvertUTF8ToJavaString(env, template_url->url());
}

ScopedJavaLocalRef<jstring> JNI_TemplateUrl_GetNewTabURL(
    JNIEnv* env,
    jlong template_url_ptr) {
  TemplateURL* template_url = ToTemplateURL(template_url_ptr);
  return base::android::ConvertUTF8ToJavaString(env,
                                                template_url->new_tab_url());
}

static jni_zero::ScopedJavaLocalRef<jbyteArray>
JNI_TemplateUrl_GetBuiltInSearchEngineIcon(JNIEnv* env,
                                           jlong template_url_ptr) {
#if BUILDFLAG(ENABLE_BUILTIN_SEARCH_PROVIDER_ASSETS)
  TemplateURL* template_url = ToTemplateURL(template_url_ptr);
  // This would be better served by ResourcesUtil::GetThemeResourceId(), but
  // the symbol appears to be unreachable from the ios/chrome/browser.
  std::string resource_name = template_url->GetBuiltinImageResourceId();
  int res_id = 0;

  auto resource_it = std::ranges::find_if(
      kSearchEnginesScaledResources,
      [&](const auto& resource) { return resource.path == resource_name; });

  // Note: it is possible to have no resource id for a prepopulated search
  // engine that was selected from a country outside of EEA countries.
  if (resource_it != std::end(kSearchEnginesScaledResources)) {
    res_id = resource_it->id;
  }

  if (res_id) {
    return base::android::ToJavaByteArray(
        env,
        ui::ResourceBundle::GetSharedInstance().GetRawDataResource(res_id));
  }
#endif
  return {};
}
