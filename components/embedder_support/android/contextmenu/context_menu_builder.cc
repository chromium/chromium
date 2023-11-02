// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/android/contextmenu/context_menu_builder.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/embedder_support/android/context_menu_jni_headers/ContextMenuParams_jni.h"
#include "content/public/browser/context_menu_params.h"
#include "third_party/blink/public/common/context_menu_data/context_menu_data.h"
#include "url/android/gurl_android.h"

using base::android::ConvertUTF16ToJavaString;

namespace context_menu {

base::android::ScopedJavaGlobalRef<jobject> BuildJavaContextMenuParams(
    const content::ContextMenuParams& params) {
  GURL sanitizedReferrer =
      (params.frame_url.is_empty() ? params.page_url : params.frame_url)
          .GetAsReferrer();

  bool can_save = params.media_flags & blink::ContextMenuData::kMediaCanSave;
  JNIEnv* env = base::android::AttachCurrentThread();
  std::u16string title_text =
      (params.title_text.empty() ? params.alt_text : params.title_text);

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
          params.y, params.source_type, params.opened_from_highlight));
}

content::ContextMenuParams* ContextMenuParamsFromJavaObject(
    const base::android::JavaRef<jobject>& jcontext_menu_params) {
  return reinterpret_cast<content::ContextMenuParams*>(
      Java_ContextMenuParams_getNativePointer(
          base::android::AttachCurrentThread(), jcontext_menu_params));
}

}  // namespace context_menu
