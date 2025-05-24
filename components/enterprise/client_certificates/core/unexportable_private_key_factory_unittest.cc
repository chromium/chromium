// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/unexportable_private_key_factory.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/enterprise/client_certificates/core/constants.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/core/private_key_types.h"
#include "components/enterprise/client_certificates/core/scoped_ssl_key_converter.h"
#include "net/ssl/ssl_private_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace client_certificates {

namespace {

void ValidatePrivateKey(scoped_refptr<PrivateKey> key,
                        scoped_refptr<PrivateKey> loaded_key) {
  ASSERT_TRUE(loaded_key);
  EXPECT_NE(key, loaded_key);
  EXPECT_EQ(key->GetAlgorithm(), loaded_key->GetAlgorithm());
  EXPECT_EQ(key->GetSource(), loaded_key->GetSource());
  EXPECT_EQ(key->GetSource(), PrivateKeySource::kUnexportableKey);
  ASSERT_THAT(key->GetSubjectPublicKeyInfo(),
              testing::ElementsAreArray(loaded_key->GetSubjectPublicKeyInfo()));
  ASSERT_TRUE(key->GetSSLPrivateKey());
}

}  // namespace

TEST(UnexportablePrivateKeyFactoryTest, SupportedCreateKey_LoadKey) {
  base::test::TaskEnvironment task_environment;
  ScopedSSLKeyConverter scoped_converter;

  auto factory = UnexportablePrivateKeyFactory::TryCreate(/*config=*/{});

  ASSERT_TRUE(factory);

  base::test::TestFuture<scoped_refptr<PrivateKey>> create_key_future;
  factory->CreatePrivateKey(create_key_future.GetCallback());

  auto private_key = create_key_future.Get();
  ASSERT_TRUE(private_key);

  base::test::TestFuture<scoped_refptr<PrivateKey>> load_key_future;
  auto proto_key = private_key->ToProto();
  factory->LoadPrivateKey(std::move(proto_key), load_key_future.GetCallback());
  ValidatePrivateKey(private_key, load_key_future.Get());
}

TEST(UnexportablePrivateKeyFactoryTest, SupportedCreateKey_LoadKeyFromDict) {
  base::test::TaskEnvironment task_environment;
  ScopedSSLKeyConverter scoped_converter;

  auto factory = UnexportablePrivateKeyFactory::TryCreate(/*config=*/{});

  ASSERT_TRUE(factory);

  base::test::TestFuture<scoped_refptr<PrivateKey>> create_key_future;
  factory->CreatePrivateKey(create_key_future.GetCallback());

  auto private_key = create_key_future.Get();
  ASSERT_TRUE(private_key);

  base::test::TestFuture<scoped_refptr<PrivateKey>> load_key_future;
  auto dict_key = private_key->ToDict();
  factory->LoadPrivateKeyFromDict(dict_key, load_key_future.GetCallback());
  ValidatePrivateKey(private_key, load_key_future.Get());

  base::test::TestFuture<scoped_refptr<PrivateKey>>
      load_key_fails_future_invalid_key;
  dict_key.Set(kKey, "");
  dict_key.Set(kKeySource,
               static_cast<int>(PrivateKeySource::kUnexportableKey));
  factory->LoadPrivateKeyFromDict(
      dict_key, load_key_fails_future_invalid_key.GetCallback());
  EXPECT_FALSE(load_key_fails_future_invalid_key.Get());
}

TEST(UnexportablePrivateKeyFactoryTest, UnsupportedCreateKey) {
  ScopedSSLKeyConverter scoped_converter(/*supports_unexportable=*/false);

  auto factory = UnexportablePrivateKeyFactory::TryCreate(/*config=*/{});

  EXPECT_FALSE(factory);
}

}  // namespace client_certificates
