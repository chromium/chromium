// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/browser/nacl_validation_cache.h"

#include "base/containers/span.h"
#include "base/pickle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nacl {

const char key1[65] =
    "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF";
const char key2[65] =
    "a 64-byte string of various junk................................";
const char sig1[33] = "0123456789ABCDEF0123456789ABCDEF";
const char sig2[33] = "a 32-byte string of various junk";

class NaClValidationCacheTest : public ::testing::Test {
 protected:
  NaClValidationCache cache1;
  NaClValidationCache cache2;

  void SetUp() override {
    // The compiler chokes if std::string(key1) is passed directly as an arg.
    std::string key(key1);
    cache1.SetValidationCacheKey(key);
    cache2.SetValidationCacheKey(key);
  }

  bool IsIdentical(const NaClValidationCache& a,
                   const NaClValidationCache& b) const {
    if (a.GetValidationCacheKey() != b.GetValidationCacheKey())
      return false;
    if (a.size() != b.size())
      return false;
    return a.GetContents() == b.GetContents();
  }
};

TEST_F(NaClValidationCacheTest, Sanity) {
  ASSERT_EQ(0u, cache1.size());
  ASSERT_FALSE(cache1.QueryKnownToValidate(sig1, true));
  ASSERT_FALSE(cache1.QueryKnownToValidate(sig2, true));
}

TEST_F(NaClValidationCacheTest, Sig1) {
  cache1.SetKnownToValidate(sig1);
  ASSERT_EQ(1, (int) cache1.size());
  ASSERT_TRUE(cache1.QueryKnownToValidate(sig1, true));
  ASSERT_FALSE(cache1.QueryKnownToValidate(sig2, true));
}

TEST_F(NaClValidationCacheTest, Sig2) {
  cache1.SetKnownToValidate(sig2);
  ASSERT_EQ(1, (int) cache1.size());
  ASSERT_FALSE(cache1.QueryKnownToValidate(sig1, true));
  ASSERT_TRUE(cache1.QueryKnownToValidate(sig2, true));
}

TEST_F(NaClValidationCacheTest, SigBoth) {
  cache1.SetKnownToValidate(sig1);
  cache1.SetKnownToValidate(sig2);
  ASSERT_EQ(2, (int) cache1.size());
  ASSERT_TRUE(cache1.QueryKnownToValidate(sig1, true));
  ASSERT_TRUE(cache1.QueryKnownToValidate(sig2, true));
}

TEST_F(NaClValidationCacheTest, DoubleSet) {
  cache1.SetKnownToValidate(sig1);
  cache1.SetKnownToValidate(sig1);
  ASSERT_EQ(1, (int) cache1.size());
  ASSERT_TRUE(cache1.QueryKnownToValidate(sig1, true));
}

TEST_F(NaClValidationCacheTest, EmptyIdentical) {
  ASSERT_TRUE(IsIdentical(cache1, cache2));
}

TEST_F(NaClValidationCacheTest, DifferentKeysNotIdentical) {
  std::string key(key2);
  cache2.SetValidationCacheKey(key);
  ASSERT_FALSE(IsIdentical(cache1, cache2));
}


TEST_F(NaClValidationCacheTest, DifferentSizesNotIdentical) {
  cache1.SetKnownToValidate(sig1);

  ASSERT_FALSE(IsIdentical(cache1, cache2));
}

TEST_F(NaClValidationCacheTest, SameSigsIdentical) {
  cache1.SetKnownToValidate(sig1);

  cache2.SetKnownToValidate(sig1);

  ASSERT_TRUE(IsIdentical(cache1, cache2));
}

TEST_F(NaClValidationCacheTest, DifferentSigsNotIdentical) {
  cache1.SetKnownToValidate(sig1);

  cache2.SetKnownToValidate(sig2);

  ASSERT_FALSE(IsIdentical(cache1, cache2));
}

TEST_F(NaClValidationCacheTest, InOrderIdentical) {
  cache1.SetKnownToValidate(sig1);
  cache1.SetKnownToValidate(sig2);

  cache2.SetKnownToValidate(sig1);
  cache2.SetKnownToValidate(sig2);

  ASSERT_TRUE(IsIdentical(cache1, cache2));
}

TEST_F(NaClValidationCacheTest, QueryReorders) {
  cache1.SetKnownToValidate(sig1);
  cache1.SetKnownToValidate(sig2);

  cache2.SetKnownToValidate(sig2);
  cache2.SetKnownToValidate(sig1);

  ASSERT_FALSE(IsIdentical(cache1, cache2));
  cache2.QueryKnownToValidate(sig2, true);
  ASSERT_TRUE(IsIdentical(cache1, cache2));
}

TEST_F(NaClValidationCacheTest, ForceNoReorder) {
  cache1.SetKnownToValidate(sig1);
  cache1.SetKnownToValidate(sig2);

  cache2.SetKnownToValidate(sig2);
  cache2.SetKnownToValidate(sig1);

  cache2.QueryKnownToValidate(sig2, false);
  ASSERT_FALSE(IsIdentical(cache1, cache2));
}

TEST_F(NaClValidationCacheTest, SerializeDeserialize) {
  std::string key(key2);
  cache1.SetValidationCacheKey(key);
  cache1.SetKnownToValidate(sig1);
  cache1.SetKnownToValidate(sig2);

  base::Pickle pickle;
  cache1.Serialize(&pickle);
  ASSERT_TRUE(cache2.Deserialize(&pickle));
  ASSERT_EQ(2, (int) cache2.size());
  ASSERT_TRUE(IsIdentical(cache1, cache2));
}

TEST_F(NaClValidationCacheTest, SerializeDeserializeTruncated) {
  std::string key(key2);
  cache1.SetValidationCacheKey(key);
  cache1.SetKnownToValidate(sig1);
  cache1.SetKnownToValidate(sig2);

  base::Pickle pickle;
  cache1.Serialize(&pickle);
  base::span<const uint8_t> full_payload = base::make_span(pickle);
  base::span<const uint8_t> truncated_payload =
      full_payload.subspan(0, full_payload.size() - 20);
  base::Pickle truncated = base::Pickle::WithUnownedBuffer(truncated_payload);
  ASSERT_FALSE(cache2.Deserialize(&truncated));
  ASSERT_EQ(0u, cache2.size());
}

TEST_F(NaClValidationCacheTest, DeserializeBadKey) {
  std::string key(sig1); // Too short, will cause the deserialization to error.
  cache1.SetValidationCacheKey(key);
  cache1.SetKnownToValidate(sig1);
  cache1.SetKnownToValidate(sig2);

  base::Pickle pickle;
  cache1.Serialize(&pickle);
  ASSERT_FALSE(cache2.Deserialize(&pickle));
  ASSERT_EQ(0u, cache2.size());
}

TEST_F(NaClValidationCacheTest, DeserializeNothing) {
  cache1.SetKnownToValidate(sig1);
  base::Pickle pickle =
      base::Pickle::WithUnownedBuffer(base::as_byte_span(std::string()));
  ASSERT_FALSE(cache1.Deserialize(&pickle));
  ASSERT_EQ(0u, cache1.size());
}

TEST_F(NaClValidationCacheTest, DeserializeJunk) {
  cache1.SetKnownToValidate(sig1);
  base::Pickle pickle =
      base::Pickle::WithUnownedBuffer(base::as_byte_span(key1));
  ASSERT_FALSE(cache1.Deserialize(&pickle));
  ASSERT_EQ(0u, cache1.size());
}

}
