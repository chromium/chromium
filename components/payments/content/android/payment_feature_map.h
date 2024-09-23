// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_FEATURE_MAP_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_FEATURE_MAP_H_

#include <jni.h>

#include "base/feature_list.h"

namespace payments {
namespace android {

// Android only payment features in alphabetical order:

// If enabled, then the web merchant origin and web wallet parameters will be
// omitted from the isReadyToPayRequest. See: https://crbug.com/1406655.
BASE_DECLARE_FEATURE(kOmitParametersInReadyToPay);

// If enabled, then Clank displays an alert dialog with the content of the
// IS_READY_TO_PAY intent, whenever Clank fires this intent.
BASE_DECLARE_FEATURE(kShowReadyToPayDebugInfo);

}  // namespace android
}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_FEATURE_MAP_H_
