// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/ec_private_key_factory.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/enterprise/client_certificates/core/ec_private_key.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "crypto/ec_private_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace client_certificates {

TEST(ECPrivateKeyFactoryTest, CreatePrivateKey) {
  base::test::TaskEnvironment task_environment;
  ECPrivateKeyFactory factory;

  base::test::TestFuture<scoped_refptr<PrivateKey>> test_future;
  factory.CreatePrivateKey(test_future.GetCallback());

  auto ec_private_key = test_future.Get();

  ASSERT_TRUE(ec_private_key);
  EXPECT_EQ(ec_private_key->GetAlgorithm(),
            crypto::SignatureVerifier::ECDSA_SHA256);

  auto spki_bytes = ec_private_key->GetSubjectPublicKeyInfo();
  EXPECT_GT(spki_bytes.size(), 0U);

  auto signature = ec_private_key->SignSlowly(spki_bytes);
  ASSERT_TRUE(signature.has_value());
  EXPECT_GT(signature->size(), 0U);
}

}  // namespace client_certificates
