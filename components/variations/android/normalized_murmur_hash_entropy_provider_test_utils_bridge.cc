// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>
#include "components/variations/android/test_support_jni_headers/NormalizedMurmurHashEntropyProviderTestUtilsBridge_jni.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/variations_murmur_hash.h"

// static
jint JNI_NormalizedMurmurHashEntropyProviderTestUtilsBridge_MurmurHash16(
    JNIEnv* env,
    jint seed,
    jint data) {
  return variations::internal::VariationsMurmurHash::Hash16(seed, data);
}

jdouble
JNI_NormalizedMurmurHashEntropyProviderTestUtilsBridge_GetEntropyForTrial(
    JNIEnv* env,
    jint randomization_seed,
    jint j_entropy_value,
    jint j_entropy_size) {
  variations::ValueInRange entropy_value{static_cast<uint32_t>(j_entropy_value),
                                         static_cast<uint32_t>(j_entropy_size)};
  variations::NormalizedMurmurHashEntropyProvider entropy_provider(
      entropy_value);
  return entropy_provider.GetEntropyForTrial("", randomization_seed);
}
