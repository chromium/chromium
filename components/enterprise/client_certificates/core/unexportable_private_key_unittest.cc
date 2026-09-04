// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/unexportable_private_key.h"

#include <array>

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/enterprise/client_certificates/core/constants.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/core/scoped_ssl_key_converter.h"
#include "crypto/sign.h"
#include "crypto/unexportable_key.h"
#include "net/ssl/ssl_private_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace client_certificates {

TEST(UnexportablePrivateKeyTest, SupportedCreateKey) {
  base::test::TaskEnvironment task_environment;
  ScopedSSLKeyConverter scoped_converter;
  auto provider = crypto::GetUnexportableKeyProvider(/*config=*/{});
  ASSERT_TRUE(provider);

  // The mock only works with the ECDSA_SHA256 algorithm.
  std::array<crypto::sign::SignatureKind, 1> kAcceptableAlgorithms = {
      crypto::sign::ECDSA_SHA256};
  auto unexportable_key =
      provider->GenerateSigningKeySlowly(kAcceptableAlgorithms);
  ASSERT_TRUE(unexportable_key);

  auto private_key =
      base::MakeRefCounted<UnexportablePrivateKey>(std::move(unexportable_key));

  auto spki_bytes = private_key->GetSubjectPublicKeyInfo();
  EXPECT_GT(spki_bytes.size(), 0U);
  EXPECT_EQ(private_key->GetAlgorithm(), crypto::sign::ECDSA_SHA256);
  base::test::TestFuture<std::optional<std::vector<uint8_t>>> test_future;
  private_key->Sign(spki_bytes, test_future.GetCallback());
  EXPECT_TRUE(test_future.Get().has_value());

  auto proto_key = private_key->ToProto();
  EXPECT_EQ(proto_key.source(),
            client_certificates_pb::PrivateKey::PRIVATE_UNEXPORTABLE_KEY);
  EXPECT_GT(proto_key.wrapped_key().size(), 0U);

  auto dict_key = private_key->ToDict();
  EXPECT_EQ(*dict_key.FindInt(kKeySource),
            static_cast<int>(PrivateKeySource::kUnexportableKey));
  EXPECT_GT(dict_key.FindString(kKey)->size(), 0U);
}

TEST(UnexportablePrivateKeyTest, SupportedCreateKeySoftware) {
  base::test::TaskEnvironment task_environment;
  ScopedSSLKeyConverter scoped_converter;
  auto provider = crypto::GetUnexportableKeyProvider(/*config=*/{});
  ASSERT_TRUE(provider);

  // The mock only works with the ECDSA_SHA256 algorithm.
  std::array<crypto::sign::SignatureKind, 1> kAcceptableAlgorithms = {
      crypto::sign::ECDSA_SHA256};
  auto unexportable_key =
      provider->GenerateSigningKeySlowly(kAcceptableAlgorithms);
  ASSERT_TRUE(unexportable_key);

  auto private_key = base::MakeRefCounted<UnexportablePrivateKey>(
      std::move(unexportable_key), PrivateKeySource::kOsSoftwareKey);

  auto spki_bytes = private_key->GetSubjectPublicKeyInfo();
  EXPECT_GT(spki_bytes.size(), 0U);
  EXPECT_EQ(private_key->GetAlgorithm(), crypto::sign::ECDSA_SHA256);
  base::test::TestFuture<std::optional<std::vector<uint8_t>>> test_future;
  private_key->Sign(spki_bytes, test_future.GetCallback());
  EXPECT_TRUE(test_future.Get().has_value());

  auto proto_key = private_key->ToProto();
  EXPECT_EQ(proto_key.source(),
            client_certificates_pb::PrivateKey::PRIVATE_OS_SOFTWARE_KEY);
  EXPECT_GT(proto_key.wrapped_key().size(), 0U);

  auto dict_key = private_key->ToDict();
  EXPECT_EQ(*dict_key.FindInt(kKeySource),
            static_cast<int>(PrivateKeySource::kOsSoftwareKey));
  EXPECT_GT(dict_key.FindString(kKey)->size(), 0U);
}

}  // namespace client_certificates
