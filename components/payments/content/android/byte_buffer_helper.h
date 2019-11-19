// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_BYTE_BUFFER_HELPER_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_BYTE_BUFFER_HELPER_H_

#include <jni.h>
#include <stdint.h>
#include <vector>

#include "base/android/scoped_java_ref.h"

namespace payments {
namespace android {

// Converts a java.nio.ByteBuffer into a vector of bytes. Sample usage:
//
//  mojom::PaymentDetailsPtr details;
//  bool success = mojom::PaymentDetails::Deserialize(
//      std::move(JavaByteBufferToNativeByteVector(env, byte_buffer)),
//      &details);
std::vector<uint8_t> JavaByteBufferToNativeByteVector(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& buffer);

}  // namespace android
}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_BYTE_BUFFER_HELPER_H_
