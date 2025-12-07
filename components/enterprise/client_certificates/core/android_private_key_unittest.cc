// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/android_private_key.h"

#include <array>

#include "base/containers/to_vector.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/enterprise/client_certificates/android/browser_binding/browser_key_store_android.h"
#include "components/enterprise/client_certificates/core/constants.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/core/scoped_ssl_key_converter.h"
#include "net/ssl/ssl_private_key.h"
#include "testing/gtest/include/gtest/gtest.h"

const std::array<device::PublicKeyCredentialParams::CredentialInfo, 1>
    kEs256Allowed = {device::PublicKeyCredentialParams::CredentialInfo{
        .algorithm = base::strict_cast<int32_t>(
            device::CoseAlgorithmIdentifier::kEs256)}};

namespace client_certificates {

TEST(AndroidPrivateKeyTest, SupportedCreateKey) {
  auto bk_key_store = CreateBrowserKeyStoreInstance();

  std::vector<uint8_t> credential_id;
  for (int i = 0; i < 32; ++i) {
    credential_id.push_back(i);
  }
  auto android_key = bk_key_store->GetOrCreateBrowserKeyForCredentialId(
      credential_id, base::ToVector(kEs256Allowed));

  ASSERT_TRUE(android_key);

  auto private_key =
      base::MakeRefCounted<AndroidPrivateKey>(std::move(android_key));

  if (bk_key_store->GetDeviceSupportsHardwareKeys()) {
    EXPECT_EQ(private_key->GetSecurityLevel(),
              BrowserKey::SecurityLevel::kStrongbox);
  } else {
    EXPECT_NE(private_key->GetSecurityLevel(),
              BrowserKey::SecurityLevel::kStrongbox);
  }

  auto spki_bytes = private_key->GetSubjectPublicKeyInfo();
  EXPECT_GT(spki_bytes.size(), 0U);
  EXPECT_EQ(private_key->GetAlgorithm(),
            crypto::SignatureVerifier::ECDSA_SHA256);
  EXPECT_TRUE(private_key->SignSlowly(spki_bytes).has_value());

  auto proto_key = private_key->ToProto();
  EXPECT_EQ(proto_key.source(),
            client_certificates_pb::PrivateKey::PRIVATE_ANDROID_KEY);
  EXPECT_GT(proto_key.wrapped_key().size(), 0U);

  auto dict_key = private_key->ToDict();
  EXPECT_EQ(*dict_key.FindInt(kKeySource),
            static_cast<int>(PrivateKeySource::kAndroidKey));
  EXPECT_GT(dict_key.FindString(kKey)->size(), 0U);
}

}  // namespace client_certificates
