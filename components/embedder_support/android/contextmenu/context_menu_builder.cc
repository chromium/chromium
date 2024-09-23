// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/android/contextmenu/context_menu_builder.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/unguessable_token_android.h"
#include "base/unguessable_token.h"
#include "content/public/browser/android/additional_navigation_params_android.h"
#include "content/public/browser/android/impression_android.h"
#include "content/public/browser/context_menu_params.h"
#include "third_party/blink/public/common/context_menu_data/context_menu_data.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/embedder_support/android/context_menu_jni_headers/ContextMenuParams_jni.h"

using base::android::ConvertUTF16ToJavaString;

namespace context_menu {

base::android::ScopedJavaGlobalRef<jobject> BuildJavaContextMenuParams(
    const content::ContextMenuParams& params,
    int initiator_process_id,
    std::optional<base::UnguessableToken> initiator_frame_token) {
  GURL sanitizedReferrer =
      (params.frame_url.is_empty() ? params.page_url : params.frame_url)
          .GetAsReferrer();

  bool can_save = params.media_flags & blink::ContextMenuData::kMediaCanSave;
  JNIEnv* env = base::android::AttachCurrentThread();
  std::u16string title_text =
      (params.title_text.empty() ? params.alt_text : params.title_text);

  std::optional<base::UnguessableToken> attribution_src_token;
  if (initiator_frame_token && params.impression) {
    attribution_src_token = params.impression->attribution_src_token.value();
  }

  base::android::ScopedJavaLocalRef<jobject> additional_navigation_params;
  if (initiator_frame_token) {
    additional_navigation_params =
        content::CreateJavaAdditionalNavigationParams(
            env, initiator_frame_token.value(), initiator_process_id,
            attribution_src_token);
  }

  return base::android::ScopedJavaGlobalRef<jobject>(
      Java_ContextMenuParams_create(
          env, reinterpret_cast<intptr_t>(&params),
          static_cast<int>(params.media_type),
          url::GURLAndroid::FromNativeGURL(env, params.page_url),
          url::GURLAndroid::FromNativeGURL(env, params.link_url),
          ConvertUTF16ToJavaString(env, params.link_text),
          url::GURLAndroid::FromNativeGURL(env, params.unfiltered_link_url),
          url::GURLAndroid::FromNativeGURL(env, params.src_url),
          ConvertUTF16ToJavaString(env, title_text),
          url::GURLAndroid::FromNativeGURL(env, sanitizedReferrer),
          static_cast<int>(params.referrer_policy), can_save, params.x,
          params.y, params.source_type, params.opened_from_highlight,
          additional_navigation_params));
}

content::ContextMenuParams* ContextMenuParamsFromJavaObject(
    const base::android::JavaRef<jobject>& jcontext_menu_params) {
  return reinterpret_cast<content::ContextMenuParams*>(
      Java_ContextMenuParams_getNativePointer(
          base::android::AttachCurrentThread(), jcontext_menu_params));
}

}  // namespace context_menu
