// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/browser_binding/browser_bound_key_store.h"

#include <stdint.h>

#include <vector>

#include "base/containers/to_vector.h"
#include "base/memory/scoped_refptr.h"
#include "components/payments/content/browser_binding/browser_bound_key.h"
#include "components/payments/content/browser_binding/browser_bound_key_android.h"
#include "components/payments/content/browser_binding/browser_bound_key_store.h"
#include "device/fido/public_key_credential_params.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

namespace {

using ::testing::AllOf;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Pointee;
using ::testing::Property;

constexpr std::array<device::PublicKeyCredentialParams::CredentialInfo, 1>
    kEs256Allowed = {device::PublicKeyCredentialParams::CredentialInfo{
        .algorithm = base::strict_cast<int32_t>(
            device::CoseAlgorithmIdentifier::kEs256)}};

using BrowserBoundKeyStoreAndroidTest = ::testing::Test;

TEST_F(BrowserBoundKeyStoreAndroidTest,
       ConvertsBrowserBoundKeyNullJavaRefToNullPtr) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  jni_zero::ScopedJavaLocalRef<jobject> bbk_java_null;

  std::unique_ptr<BrowserBoundKeyAndroid> bbk_cxx =
      jni_zero::FromJniType<std::unique_ptr<BrowserBoundKeyAndroid>>(
          env, bbk_java_null);

  EXPECT_EQ(bbk_cxx, nullptr);
}

TEST_F(BrowserBoundKeyStoreAndroidTest, GetsPublicKeyWhenSupportsStrongBox) {
  scoped_refptr<BrowserBoundKeyStore> bbk_store =
      GetBrowserBoundKeyStoreInstance();
  ASSERT_TRUE(bbk_store);
  if (!bbk_store->GetDeviceSupportsHardwareKeys()) {
    // StrongBox is required for this test.
    GTEST_SKIP();
  }
  std::vector<uint8_t> bbk_id{1, 2, 3, 4};

  std::unique_ptr<BrowserBoundKey> bbk =
      bbk_store->GetOrCreateBrowserBoundKeyForCredentialId(
          std::move(bbk_id), base::ToVector(kEs256Allowed));

  EXPECT_THAT(
      bbk,
      AllOf(NotNull(), Pointee(Property(&BrowserBoundKey::GetPublicKeyAsCoseKey,
                                        Not(IsEmpty())))));

  // Clean up by removing the bbk.
  if (bbk) {
    bbk_store->DeleteBrowserBoundKey(bbk->GetIdentifier());
  }
}

TEST_F(BrowserBoundKeyStoreAndroidTest, DoesNotGetBbkWhenNoStrongBoxSupport) {
  scoped_refptr<BrowserBoundKeyStore> bbk_store =
      GetBrowserBoundKeyStoreInstance();
  ASSERT_TRUE(bbk_store);
  if (bbk_store->GetDeviceSupportsHardwareKeys()) {
    // This test does not run with StrongBox.
    GTEST_SKIP();
  }
  std::vector<uint8_t> bbk_id{1, 2, 3, 4};

  std::unique_ptr<BrowserBoundKey> bbk =
      bbk_store->GetOrCreateBrowserBoundKeyForCredentialId(
          std::move(bbk_id), base::ToVector(kEs256Allowed));

  EXPECT_EQ(bbk, nullptr);
}

}  // namespace

}  // namespace payments
