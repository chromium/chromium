// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_ANDROID_H_
#define COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_ANDROID_H_

#include <memory>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "components/payments/content/browser_binding/browser_bound_key.h"
#include "third_party/jni_zero/default_conversions.h"
#include "third_party/jni_zero/jni_zero.h"

namespace payments {

// Implements BrowserBoundKey for Android.
class BrowserBoundKeyAndroid : public BrowserBoundKey {
 public:
  // `impl` must not be a null Java reference.
  explicit BrowserBoundKeyAndroid(const jni_zero::JavaRef<jobject>& impl);
  ~BrowserBoundKeyAndroid() override;
  std::vector<uint8_t> GetIdentifier() const override;
  std::vector<uint8_t> Sign(const std::vector<uint8_t>& client_data) override;
  std::vector<uint8_t> GetPublicKeyAsCoseKey() const override;

 private:
  jni_zero::ScopedJavaGlobalRef<jobject> impl_;
};

}  // namespace payments

namespace jni_zero {

template <>
inline std::unique_ptr<payments::BrowserBoundKeyAndroid> FromJniType(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj) {
  if (!obj) {
    return nullptr;
  }
  return std::make_unique<payments::BrowserBoundKeyAndroid>(obj);
}

}  // namespace jni_zero

#endif  // COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_ANDROID_H_
