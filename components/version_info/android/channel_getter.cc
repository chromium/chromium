// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/version_info/android/channel_getter.h"

#include "components/version_info/android/version_constants_bridge_jni/VersionConstantsBridge_jni.h"

namespace version_info {
namespace android {

Channel GetChannel() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return static_cast<Channel>(Java_VersionConstantsBridge_getChannel(env));
}

}  // namespace android
}  // namespace version_info
