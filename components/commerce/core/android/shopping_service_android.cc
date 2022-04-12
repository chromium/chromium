// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/android/shopping_service_android.h"

#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/shopping_service_jni_headers/ShoppingService_jni.h"

namespace commerce {

ShoppingServiceAndroid::ShoppingServiceAndroid(ShoppingService* service)
    : shopping_service_(service) {
  java_ref_.Reset(Java_ShoppingService_create(
      base::android::AttachCurrentThread(), reinterpret_cast<jlong>(this)));
}

ShoppingServiceAndroid::~ShoppingServiceAndroid() {
  Java_ShoppingService_destroy(base::android::AttachCurrentThread(), java_ref_);
}

}  // namespace commerce
