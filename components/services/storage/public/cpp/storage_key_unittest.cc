// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/cpp/storage_key.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace storage {

// Test when a constructed StorageKey object should be considered valid/opaque.
TEST(StorageStorageKeyTest, ConstructionValidity) {
  StorageKey empty = StorageKey();
  EXPECT_TRUE(empty.opaque());

  url::Origin valid_origin = url::Origin::Create(GURL("https://example.com"));
  StorageKey valid = StorageKey(valid_origin);
  EXPECT_FALSE(valid.opaque());

  url::Origin invalid_origin =
      url::Origin::Create(GURL("I'm not a valid URL."));
  StorageKey invalid = StorageKey(invalid_origin);
  EXPECT_TRUE(invalid.opaque());
}

// Test that StorageKeys are/aren't equivalent as expected.
TEST(StorageStorageKeyTest, Equivalance) {
  url::Origin origin1 = url::Origin::Create(GURL("https://example.com"));
  url::Origin origin2 = url::Origin::Create(GURL("https://test.example"));
  url::Origin origin3 = url::Origin();
  url::Origin origin4 =
      url::Origin();  // Creates a different opaque origin than origin3.

  StorageKey key1_origin1 = StorageKey(origin1);
  StorageKey key2_origin1 = StorageKey(origin1);
  StorageKey key3_origin2 = StorageKey(origin2);

  StorageKey key4_origin3 = StorageKey(origin3);
  StorageKey key5_origin3 = StorageKey(origin3);
  StorageKey key6_origin4 = StorageKey(origin4);
  EXPECT_TRUE(key4_origin3.opaque());
  EXPECT_TRUE(key5_origin3.opaque());
  EXPECT_TRUE(key6_origin4.opaque());

  // All are equivalent to themselves
  EXPECT_EQ(key1_origin1, key1_origin1);
  EXPECT_EQ(key2_origin1, key2_origin1);
  EXPECT_EQ(key3_origin2, key3_origin2);
  EXPECT_EQ(key4_origin3, key4_origin3);
  EXPECT_EQ(key5_origin3, key5_origin3);
  EXPECT_EQ(key6_origin4, key6_origin4);

  // StorageKeys created from the same origins are equivalent.
  EXPECT_EQ(key1_origin1, key2_origin1);
  EXPECT_EQ(key4_origin3, key5_origin3);

  // StorageKeys created from different origins are not equivalent.
  EXPECT_NE(key1_origin1, key3_origin2);
  EXPECT_NE(key4_origin3, key6_origin4);
}

// Test that StorageKeys Serialize to the expected value.
TEST(StorageStorageKeyTest, Serialize) {
  std::string example = "https://example.com/";
  std::string example_no_trailing_slash = "https://example.com";
  std::string test = "https://test.example/";
  StorageKey key1 = StorageKey(url::Origin::Create(GURL(example)));
  StorageKey key2 =
      StorageKey(url::Origin::Create(GURL(example_no_trailing_slash)));
  StorageKey key3 = StorageKey(url::Origin::Create(GURL(test)));

  EXPECT_EQ(key1.Serialize(), example);
  // URLs will be normalized into the same spec format.
  EXPECT_EQ(key2.Serialize(), example);
  EXPECT_EQ(key3.Serialize(), test);
}

// Test that deserialized StorageKeys are valid/opaque as expected.
TEST(StorageStorageKeyTest, Deserialize) {
  std::string example = "https://example.com/";
  std::string test = "https://test.example/";
  std::string wrong = "I'm not a valid URL.";

  StorageKey key1 = StorageKey::Deserialize(example);
  StorageKey key2 = StorageKey::Deserialize(test);
  StorageKey key3 = StorageKey::Deserialize(wrong);
  StorageKey key4 = StorageKey::Deserialize(std::string());

  EXPECT_FALSE(key1.opaque());
  EXPECT_FALSE(key2.opaque());
  EXPECT_TRUE(key3.opaque());
  EXPECT_TRUE(key4.opaque());
}

// Test that a StorageKey, constructed by deserializing another serialized
// StorageKey, is equivalent to the original.
TEST(StorageStorageKeyTest, SerializeDeserialize) {
  url::Origin origin1 = url::Origin::Create(GURL("https://example.com"));
  url::Origin origin2 = url::Origin::Create(GURL("https://test.example"));

  StorageKey key1 = StorageKey(origin1);
  StorageKey key2 = StorageKey(origin2);

  std::string key1_string = key1.Serialize();
  std::string key2_string = key2.Serialize();

  StorageKey key1_deserialized = StorageKey::Deserialize(key1_string);
  StorageKey key2_deserialized = StorageKey::Deserialize(key2_string);

  EXPECT_EQ(key1, key1_deserialized);
  EXPECT_EQ(key2, key2_deserialized);
}

}  // namespace storage
