// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/local_data_description.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_array.h"
#include "components/sync/android/jni_headers/LocalDataDescription_jni.h"

using base::android::ToJavaArrayOfStrings;
#endif

namespace syncer {

LocalDataDescription::LocalDataDescription() = default;

LocalDataDescription::LocalDataDescription(const LocalDataDescription&) =
    default;

LocalDataDescription& LocalDataDescription::operator=(
    const LocalDataDescription&) = default;

LocalDataDescription::LocalDataDescription(LocalDataDescription&&) = default;

LocalDataDescription& LocalDataDescription::operator=(LocalDataDescription&&) =
    default;

LocalDataDescription::~LocalDataDescription() = default;

void PrintTo(const LocalDataDescription& desc, std::ostream* os) {
  *os << "{ item_count:" << desc.item_count << ", domains:[";
  for (const auto& domain : desc.domains) {
    *os << domain << ",";
  }
  *os << "], domain_count:" << desc.domain_count;
}

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject> ConvertToJavaLocalDataDescription(
    JNIEnv* env,
    const LocalDataDescription& local_data_description) {
  return Java_LocalDataDescription_Constructor(
      env, local_data_description.item_count,
      base::android::ToJavaArrayOfStrings(env, local_data_description.domains),
      local_data_description.domain_count);
}
#endif

}  // namespace syncer
