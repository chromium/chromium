// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/android/facilitated_payments_app_info_list_android.h"

#include "base/android/jni_android.h"

namespace payments::facilitated {

FacilitatedPaymentsAppInfoListAndroid::FacilitatedPaymentsAppInfoListAndroid(
    base::android::ScopedJavaLocalRef<jobjectArray> apps)
    : apps_(std::move(apps)) {}

FacilitatedPaymentsAppInfoListAndroid::
    ~FacilitatedPaymentsAppInfoListAndroid() = default;

size_t FacilitatedPaymentsAppInfoListAndroid::Size() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  // `GetArrayLength()` returns a signed integer that is always non-negative, so
  // static-casting it to an unsigned integer is safe.
  return static_cast<size_t>(env->GetArrayLength(apps_.obj()));
}

const base::android::ScopedJavaLocalRef<jobjectArray>&
FacilitatedPaymentsAppInfoListAndroid::GetJavaArray() const {
  return apps_;
}

}  // namespace payments::facilitated
