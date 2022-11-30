// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crypto/signature_cache.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace {

TEST(SignatureCache, Hit) {
  const std::string private_key1("key1");
  const std::string hash1("hash1");
  const std::string hash2("hash2");
  const std::string signature1("signature1");
  const std::string signature2("signature2");
  SignatureCache cache;
  cache.Put(private_key1, hash1, signature1);
  cache.Put(private_key1, hash2, signature2);
  ASSERT_EQ(signature1, cache.Get(private_key1, hash1));
  ASSERT_EQ(signature2, cache.Get(private_key1, hash2));
}

TEST(SignatureCache, Miss) {
  const std::string private_key1("key1");
  const std::string hash1("hash1");
  const std::string hash2("hash2");
  const std::string signature1("signature1");
  SignatureCache cache;
  cache.Put(private_key1, hash1, signature1);
  ASSERT_EQ("", cache.Get(private_key1, hash2));
}

TEST(SignatureCache, NewPrivateKeyHit) {
  const std::string private_key1("key1");
  const std::string private_key2("key2");
  const std::string hash1("hash1");
  const std::string signature1("signature1");
  SignatureCache cache;
  cache.Put(private_key1, hash1, signature1);
  cache.Put(private_key2, hash1, signature1);
  ASSERT_EQ(signature1, cache.Get(private_key2, hash1));
}

TEST(SignatureCache, NewPrivateKeyMiss) {
  const std::string private_key1("key1");
  const std::string private_key2("key2");
  const std::string hash1("hash1");
  const std::string signature1("signature1");
  SignatureCache cache;
  cache.Put(private_key1, hash1, signature1);
  ASSERT_EQ("", cache.Get(private_key2, hash1));
  cache.Put(private_key2, hash1, signature1);
  ASSERT_NE(signature1, cache.Get(private_key1, hash1));
}

}  // namespace
}  // namespace chromecast
