// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/android/browser_binding/browser_key_store_android.h"

#include <stdint.h>

#include <vector>

#include "base/containers/to_vector.h"
#include "base/memory/scoped_refptr.h"
#include "components/enterprise/client_certificates/android/browser_binding/browser_key.h"
#include "components/enterprise/client_certificates/android/browser_binding/browser_key_android.h"
#include "components/enterprise/client_certificates/android/browser_binding/browser_key_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace client_certificates {

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

using BrowserKeyStoreAndroidTest = ::testing::Test;

TEST_F(BrowserKeyStoreAndroidTest, ConvertsBrowserKeyNullJavaRefToNullPtr) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  jni_zero::ScopedJavaLocalRef<jobject> bk_java_null;

  std::unique_ptr<BrowserKeyAndroid> bk_cxx =
      jni_zero::FromJniType<std::unique_ptr<BrowserKeyAndroid>>(env,
                                                                bk_java_null);

  EXPECT_EQ(bk_cxx, nullptr);
}

TEST_F(BrowserKeyStoreAndroidTest, GetsPublicKeyWhenSupportsStrongBox) {
  scoped_refptr<BrowserKeyStore> bk_store = CreateBrowserKeyStoreInstance();
  ASSERT_TRUE(bk_store);
  if (!bk_store->GetDeviceSupportsHardwareKeys()) {
    // StrongBox is required for this test.
    GTEST_SKIP();
  }
  std::vector<uint8_t> bk_id{1, 2, 3, 4};

  std::unique_ptr<BrowserKey> bk =
      bk_store->GetOrCreateBrowserKeyForCredentialId(
          std::move(bk_id), base::ToVector(kEs256Allowed));

  EXPECT_FALSE(bk->GetPrivateKey().is_null());
  EXPECT_TRUE(bk->GetSSLPrivateKey());
  EXPECT_FALSE(bk->GetPublicKeyAsSPKI().empty());
  EXPECT_FALSE(bk->GetIdentifier().empty());
  auto signature = bk->Sign(std::vector<uint8_t>{1, 2, 3});
  EXPECT_TRUE(signature.has_value());
  EXPECT_FALSE(signature.value().empty());
  EXPECT_EQ(bk->GetSecurityLevel(), BrowserKey::SecurityLevel::kStrongbox);

  // Clean up by removing the bk.
  if (bk) {
    bk_store->DeleteBrowserKey(bk->GetIdentifier());
  }
}

TEST_F(BrowserKeyStoreAndroidTest, GetsPublicKeyWhenNoStrongBoxSupport) {
  scoped_refptr<BrowserKeyStore> bk_store = CreateBrowserKeyStoreInstance();
  ASSERT_TRUE(bk_store);
  if (bk_store->GetDeviceSupportsHardwareKeys()) {
    // StrongBox must be unsupported for this test.
    GTEST_SKIP();
  }
  std::vector<uint8_t> bk_id{1, 2, 3, 4};

  std::unique_ptr<BrowserKey> bk =
      bk_store->GetOrCreateBrowserKeyForCredentialId(
          std::move(bk_id), base::ToVector(kEs256Allowed));

  EXPECT_FALSE(bk->GetPrivateKey().is_null());
  EXPECT_TRUE(bk->GetSSLPrivateKey());
  EXPECT_FALSE(bk->GetPublicKeyAsSPKI().empty());
  EXPECT_FALSE(bk->GetIdentifier().empty());
  auto signature = bk->Sign(std::vector<uint8_t>{1, 2, 3});
  EXPECT_TRUE(signature.has_value());
  EXPECT_FALSE(signature.value().empty());
  EXPECT_NE(bk->GetSecurityLevel(), BrowserKey::SecurityLevel::kStrongbox);

  // Clean up by removing the bk.
  if (bk) {
    bk_store->DeleteBrowserKey(bk->GetIdentifier());
  }
}

}  // namespace

}  // namespace client_certificates
