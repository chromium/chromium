// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_FEATURE_LIST_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_FEATURE_LIST_H_

#include <base/feature_list.h>
#include <jni.h>

namespace payments {
namespace android {

// Android only payment features in alphabetical order:
extern const base::Feature kAndroidAppPaymentUpdateEvents;
extern const base::Feature kScrollToExpandPaymentHandler;

}  // namespace android
}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_FEATURE_LIST_H_
