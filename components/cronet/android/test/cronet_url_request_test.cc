// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/load_flags.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/cronet/android/cronet_tests_jni_headers/CronetUrlRequestTest_jni.h"

static jint JNI_CronetUrlRequestTest_GetConnectionMigrationDisableLoadFlag(
    JNIEnv* env) {
  return net::LOAD_DISABLE_CONNECTION_MIGRATION_TO_CELLULAR;
}
