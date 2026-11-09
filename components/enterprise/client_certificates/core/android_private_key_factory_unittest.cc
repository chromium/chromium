// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/android_private_key_factory.h"

#include <cstdint>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/memory/scoped_refptr.h"
#include "base/rand_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "components/enterprise/client_certificates/android/browser_binding/browser_key_store.h"
#include "components/enterprise/client_certificates/core/constants.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/core/private_key_types.h"
#include "components/enterprise/client_certificates/core/scoped_ssl_key_converter.h"
#include "components/enterprise/client_certificates/proto/client_certificates_database.pb.h"
#include "net/ssl/ssl_private_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace client_certificates {

namespace {

void ValidatePrivateKey(scoped_refptr<PrivateKey> key,
                        scoped_refptr<PrivateKey> loaded_key) {
  EXPECT_NE(key, loaded_key);
  EXPECT_EQ(key->GetAlgorithm(), loaded_key->GetAlgorithm());
  EXPECT_EQ(key->GetSource(), loaded_key->GetSource());
  EXPECT_EQ(key->GetSource(), PrivateKeySource::kAndroidKey);
  ASSERT_THAT(key->GetSubjectPublicKeyInfo(),
              testing::ElementsAreArray(loaded_key->GetSubjectPublicKeyInfo()));
  ASSERT_TRUE(key->GetSSLPrivateKey());
}

// Returns the keystore identifier `key` was created under.
std::vector<uint8_t> GetIdentifier(const PrivateKey& key) {
  const std::string wrapped_key = key.ToProto().wrapped_key();
  return std::vector<uint8_t>(wrapped_key.begin(), wrapped_key.end());
}

// Deletes the keystore entry backing `key`.
void DeleteKeyStoreEntry(const PrivateKey& key) {
  CreateBrowserKeyStoreInstance()->DeleteBrowserKey(GetIdentifier(key));
}

// Returns an identifier for which no keystore entry exists.
std::vector<uint8_t> UnknownIdentifier() {
  std::vector<uint8_t> identifier(32);
  base::RandBytes(identifier);
  return identifier;
}

client_certificates_pb::PrivateKey ToProto(
    const std::vector<uint8_t>& identifier) {
  client_certificates_pb::PrivateKey proto_key;
  proto_key.set_source(ToProtoKeySource(PrivateKeySource::kAndroidKey));
  proto_key.set_wrapped_key(std::string(identifier.begin(), identifier.end()));
  return proto_key;
}

base::DictValue ToDict(const std::vector<uint8_t>& identifier) {
  base::DictValue dict_key;
  dict_key.Set(kKeySource, static_cast<int>(PrivateKeySource::kAndroidKey));
  dict_key.Set(kKey, base::Base64Encode(identifier));
  return dict_key;
}

}  // namespace

TEST(AndroidPrivateKeyFactoryTest, TestCreateFactory) {
  auto factory = AndroidPrivateKeyFactory::TryCreate();
  // Expect that the factory is created successfully.
  EXPECT_TRUE(factory);
}

TEST(AndroidPrivateKeyFactoryTest, SupportedCreateKey_LoadKey) {
  base::test::TaskEnvironment task_environment;
  ScopedSSLKeyConverter scoped_converter;

  auto factory = AndroidPrivateKeyFactory::TryCreate();

  EXPECT_NE(factory, nullptr);

  base::test::TestFuture<scoped_refptr<PrivateKey>> create_key_future;
  factory->CreatePrivateKey(create_key_future.GetCallback());

  auto private_key = create_key_future.Get();
  ASSERT_TRUE(private_key);

  base::test::TestFuture<scoped_refptr<PrivateKey>> load_key_future;
  auto proto_key = private_key->ToProto();
  factory->LoadPrivateKey(std::move(proto_key), load_key_future.GetCallback());
  ValidatePrivateKey(private_key, load_key_future.Get());

  DeleteKeyStoreEntry(*private_key);
}

TEST(AndroidPrivateKeyFactoryTest, SupportedCreateKey_LoadKeyFromDict) {
  base::test::TaskEnvironment task_environment;
  ScopedSSLKeyConverter scoped_converter;

  auto factory = AndroidPrivateKeyFactory::TryCreate();

  EXPECT_NE(factory, nullptr);

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
  dict_key.Set(kKeySource, static_cast<int>(PrivateKeySource::kAndroidKey));
  factory->LoadPrivateKeyFromDict(
      dict_key, load_key_fails_future_invalid_key.GetCallback());
  EXPECT_FALSE(load_key_fails_future_invalid_key.Get());

  DeleteKeyStoreEntry(*private_key);
}

TEST(AndroidPrivateKeyFactoryTest,
     LoadPrivateKey_UnknownIdentifier_ReturnsNull) {
  base::test::TaskEnvironment task_environment;
  ScopedSSLKeyConverter scoped_converter;
  auto factory = AndroidPrivateKeyFactory::TryCreate();

  base::test::TestFuture<scoped_refptr<PrivateKey>> load_key_future;
  factory->LoadPrivateKey(ToProto(UnknownIdentifier()),
                          load_key_future.GetCallback());
  EXPECT_FALSE(load_key_future.Get());
}

TEST(AndroidPrivateKeyFactoryTest,
     LoadPrivateKeyFromDict_UnknownIdentifier_ReturnsNull) {
  base::test::TaskEnvironment task_environment;
  ScopedSSLKeyConverter scoped_converter;
  auto factory = AndroidPrivateKeyFactory::TryCreate();

  base::test::TestFuture<scoped_refptr<PrivateKey>> load_key_future;
  factory->LoadPrivateKeyFromDict(ToDict(UnknownIdentifier()),
                                  load_key_future.GetCallback());
  EXPECT_FALSE(load_key_future.Get());
}

// Loading must not recreate a key whose keystore entry is gone: the stored
// certificate would no longer match it.
TEST(AndroidPrivateKeyFactoryTest, LoadPrivateKey_DeletedKey_ReturnsNull) {
  base::test::TaskEnvironment task_environment;
  ScopedSSLKeyConverter scoped_converter;
  auto factory = AndroidPrivateKeyFactory::TryCreate();

  base::test::TestFuture<scoped_refptr<PrivateKey>> create_key_future;
  factory->CreatePrivateKey(create_key_future.GetCallback());
  auto private_key = create_key_future.Get();
  ASSERT_TRUE(private_key);

  DeleteKeyStoreEntry(*private_key);

  base::test::TestFuture<scoped_refptr<PrivateKey>> load_key_future;
  factory->LoadPrivateKey(private_key->ToProto(),
                          load_key_future.GetCallback());
  EXPECT_FALSE(load_key_future.Get());

  base::test::TestFuture<scoped_refptr<PrivateKey>> load_dict_key_future;
  factory->LoadPrivateKeyFromDict(private_key->ToDict(),
                                  load_dict_key_future.GetCallback());
  EXPECT_FALSE(load_dict_key_future.Get());
}

}  // namespace client_certificates
