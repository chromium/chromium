// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_deep_link_payload_conversions.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/signin/public/android/jni_headers/SigninDeepLinkPayload_jni.h"

namespace jni_zero {
template <>
base::android::ScopedJavaLocalRef<jobject>
ToJniType<signin::SigninDeepLinkPayload>(
    JNIEnv* env,
    const signin::SigninDeepLinkPayload& input) {
  CHECK(input.HasAllRequiredFields());
  const int32_t entry_point_id =
      static_cast<int32_t>(input.entry_point_id.value());
  return SigninDeepLinkPayloadJni::New(env, entry_point_id,
                                       input.email.value());
}
}  // namespace jni_zero
