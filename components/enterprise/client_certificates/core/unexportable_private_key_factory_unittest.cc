// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/unexportable_private_key_factory.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/core/scoped_ssl_key_converter.h"
#include "net/ssl/ssl_private_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace client_certificates {

TEST(UnexportablePrivateKeyFactoryTest, SupportedCreateKey_LoadKey) {
  base::test::TaskEnvironment task_environment;
  ScopedSSLKeyConverter scoped_converter;

  auto factory = UnexportablePrivateKeyFactory::TryCreate(/*config=*/{});

  ASSERT_TRUE(factory);

  base::test::TestFuture<scoped_refptr<PrivateKey>> test_future;
  factory->CreatePrivateKey(test_future.GetCallback());

  auto private_key = test_future.Get();
  ASSERT_TRUE(private_key);

  base::test::TestFuture<scoped_refptr<PrivateKey>> other_test_future;
  auto proto_key = private_key->ToProto();
  factory->LoadPrivateKey(std::move(proto_key),
                          other_test_future.GetCallback());

  auto second_private_key = other_test_future.Get();
  ASSERT_TRUE(second_private_key);
  EXPECT_NE(private_key, second_private_key);
  EXPECT_EQ(private_key->GetAlgorithm(), second_private_key->GetAlgorithm());
  ASSERT_THAT(
      private_key->GetSubjectPublicKeyInfo(),
      testing::ElementsAreArray(second_private_key->GetSubjectPublicKeyInfo()));
  ASSERT_TRUE(private_key->GetSSLPrivateKey());
  ASSERT_TRUE(second_private_key->GetSSLPrivateKey());
}

TEST(UnexportablePrivateKeyFactoryTest, UnsupportedCreateKey) {
  ScopedSSLKeyConverter scoped_converter(/*supports_unexportable=*/false);

  auto factory = UnexportablePrivateKeyFactory::TryCreate(/*config=*/{});

  EXPECT_FALSE(factory);
}

}  // namespace client_certificates
