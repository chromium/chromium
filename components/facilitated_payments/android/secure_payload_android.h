// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_ANDROID_SECURE_PAYLOAD_ANDROID_H_
#define COMPONENTS_FACILITATED_PAYMENTS_ANDROID_SECURE_PAYLOAD_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "components/facilitated_payments/core/browser/model/secure_payload.h"

namespace payments::facilitated {

base::android::ScopedJavaLocalRef<jobject> ConvertSecurePayloadToJavaObject(
    const SecurePayload& secure_payload);

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_ANDROID_SECURE_PAYLOAD_ANDROID_H_
