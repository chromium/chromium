// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_ANDROID_H_
#define COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_ANDROID_H_

#include <vector>

#include "components/payments/content/browser_binding/browser_bound_key.h"
#include "third_party/jni_zero/jni_zero.h"

namespace payments {

// Implements BrowserBoundKey for Android.
class BrowserBoundKeyAndroid : public BrowserBoundKey {
 public:
  explicit BrowserBoundKeyAndroid(jni_zero::ScopedJavaLocalRef<jobject> impl);
  ~BrowserBoundKeyAndroid() override;
  std::vector<uint8_t> GetIdentifier() const override;
  std::vector<uint8_t> Sign(const std::vector<uint8_t>& client_data) override;
  std::vector<uint8_t> GetPublicKeyAsCoseKey() const override;

 private:
  jni_zero::ScopedJavaGlobalRef<jobject> impl_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_ANDROID_H_
