// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_DEEP_LINK_PAYLOAD_CONVERSIONS_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_DEEP_LINK_PAYLOAD_CONVERSIONS_H_

#include "base/android/scoped_java_ref.h"
#include "components/signin/public/base/signin_deep_link_payload.h"
#include "third_party/jni_zero/jni_zero.h"

namespace jni_zero {
template <>
base::android::ScopedJavaLocalRef<jobject>
ToJniType<signin::SigninDeepLinkPayload>(
    JNIEnv* env,
    const signin::SigninDeepLinkPayload& input);
}  // namespace jni_zero

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_DEEP_LINK_PAYLOAD_CONVERSIONS_H_
