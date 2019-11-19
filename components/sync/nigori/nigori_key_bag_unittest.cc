// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/nigori_key_bag.h"

#include "components/sync/nigori/nigori.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using testing::Eq;
using testing::Ne;
using testing::SizeIs;

std::unique_ptr<Nigori> CreateTestNigori(const std::string& password) {
  return Nigori::CreateByDerivation(KeyDerivationParams::CreateForPbkdf2(),
                                    password);
}

TEST(NigoriKeyBagTest, ShouldCreateEmpty) {
  const NigoriKeyBag key_bag = NigoriKeyBag::CreateEmpty();
  EXPECT_THAT(key_bag, SizeIs(0));
  EXPECT_FALSE(key_bag.HasKey("foo"));
}

TEST(NigoriKeyBagTest, ShouldAddKeys) {
  NigoriKeyBag key_bag = NigoriKeyBag::CreateEmpty();
  ASSERT_THAT(key_bag, SizeIs(0));

  const std::string key_name1 = key_bag.AddKey(CreateTestNigori("password1"));
  EXPECT_THAT(key_name1, Ne(""));
  EXPECT_THAT(key_bag, SizeIs(1));
  EXPECT_TRUE(key_bag.HasKey(key_name1));

  const std::string key_name2 = key_bag.AddKey(CreateTestNigori("password2"));
  EXPECT_THAT(key_name2, Ne(""));
  EXPECT_THAT(key_name2, Ne(key_name1));
  EXPECT_THAT(key_bag, SizeIs(2));
  EXPECT_TRUE(key_bag.HasKey(key_name1));
  EXPECT_TRUE(key_bag.HasKey(key_name2));
}

TEST(NigoriKeyBagTest, ShouldExportKey) {
  NigoriKeyBag key_bag = NigoriKeyBag::CreateEmpty();
  const std::string key_name1 = key_bag.AddKey(CreateTestNigori("password1"));
  ASSERT_THAT(key_bag, SizeIs(1));
  ASSERT_THAT(key_name1, Ne(""));
  ASSERT_TRUE(key_bag.HasKey(key_name1));

  sync_pb::NigoriKey exported_key = key_bag.ExportKey(key_name1);

  // Callers of ExportKey() rely on the deprecated field *NOT* being populated.
  EXPECT_FALSE(exported_key.has_deprecated_name());

  // The exported key, even without name, should be importable, and the
  // resulting key name should match the original.
  EXPECT_THAT(NigoriKeyBag::CreateEmpty().AddKeyFromProto(exported_key),
              Eq(key_name1));
}

TEST(NigoriKeyBagTest, ShouldConvertEmptyToProto) {
  EXPECT_EQ(sync_pb::NigoriKeyBag().SerializeAsString(),
            NigoriKeyBag::CreateEmpty().ToProto().SerializeAsString());
}

TEST(NigoriKeyBagTest, ShouldConvertNonEmptyToProto) {
  NigoriKeyBag key_bag = NigoriKeyBag::CreateEmpty();
  const std::string key_name = key_bag.AddKey(CreateTestNigori("password1"));

  sync_pb::NigoriKeyBag proto = key_bag.ToProto();
  ASSERT_THAT(proto.key(), SizeIs(1));
  // Callers of ToProto() rely on the deprecated name field being populated,
  // since it gets exposed to the sync protocol, and hence subject to backward
  // compatibility.
  EXPECT_THAT(proto.key(0).deprecated_name(), Eq(key_name));
  EXPECT_THAT(proto.key(0).deprecated_user_key(), Ne(""));
  EXPECT_THAT(proto.key(0).encryption_key(), Ne(""));
  EXPECT_THAT(proto.key(0).mac_key(), Ne(""));
}

TEST(NigoriKeyBagTest, ShouldCreateEmptyFromProto) {
  EXPECT_THAT(NigoriKeyBag::CreateFromProto(sync_pb::NigoriKeyBag()),
              SizeIs(0));
}

TEST(NigoriKeyBagTest, ShouldCreateNonEmptyFromProto) {
  NigoriKeyBag original_key_bag = NigoriKeyBag::CreateEmpty();
  const std::string key_name1 =
      original_key_bag.AddKey(CreateTestNigori("password1"));
  const std::string key_name2 =
      original_key_bag.AddKey(CreateTestNigori("password2"));
  ASSERT_THAT(original_key_bag, SizeIs(2));

  const NigoriKeyBag restored_key_bag =
      NigoriKeyBag::CreateFromProto(original_key_bag.ToProto());
  EXPECT_THAT(restored_key_bag, SizeIs(2));
  EXPECT_TRUE(restored_key_bag.HasKey(key_name1));
  EXPECT_TRUE(restored_key_bag.HasKey(key_name2));
}

TEST(NigoriKeyBagTest, ShouldCreateNonEmptyFromPartiallyInvalidProto) {
  NigoriKeyBag original_key_bag = NigoriKeyBag::CreateEmpty();
  const std::string key_name1 =
      original_key_bag.AddKey(CreateTestNigori("password1"));
  const std::string key_name2 =
      original_key_bag.AddKey(CreateTestNigori("password2"));

  sync_pb::NigoriKeyBag malformed_proto = original_key_bag.ToProto();
  ASSERT_THAT(malformed_proto.key(), SizeIs(2));
  malformed_proto.mutable_key(1)->set_encryption_key("malformed-key");

  NigoriKeyBag restored_key_bag =
      NigoriKeyBag::CreateFromProto(malformed_proto);

  EXPECT_THAT(restored_key_bag, SizeIs(1));
  EXPECT_TRUE(restored_key_bag.HasKey(key_name1));
  EXPECT_FALSE(restored_key_bag.HasKey(key_name2));
}

TEST(NigoriKeyBagTest, ShouldClone) {
  NigoriKeyBag original_key_bag = NigoriKeyBag::CreateEmpty();
  const std::string key_name1 =
      original_key_bag.AddKey(CreateTestNigori("password1"));
  const std::string key_name2 =
      original_key_bag.AddKey(CreateTestNigori("password2"));
  ASSERT_THAT(original_key_bag, SizeIs(2));

  const NigoriKeyBag cloned_key_bag = original_key_bag.Clone();
  EXPECT_THAT(cloned_key_bag, SizeIs(2));
  EXPECT_TRUE(cloned_key_bag.HasKey(key_name1));
  EXPECT_TRUE(cloned_key_bag.HasKey(key_name2));
}

// This holds true for M79 and above, but older clients rely on the field being
// set.
TEST(NigoriKeyBagTest, ShouldIgnoreDeprecatedKeyNameProtoField) {
  NigoriKeyBag original_key_bag = NigoriKeyBag::CreateEmpty();
  const std::string real_key_name =
      original_key_bag.AddKey(CreateTestNigori("password1"));
  ASSERT_THAT(original_key_bag, SizeIs(1));

  const std::string actual_key_name_in_proto =
      NigoriKeyBag::CreateEmpty().AddKey(CreateTestNigori("password2"));

  sync_pb::NigoriKeyBag proto = original_key_bag.ToProto();
  proto.mutable_key(0)->set_deprecated_name(actual_key_name_in_proto);

  NigoriKeyBag restored_key_bag = NigoriKeyBag::CreateFromProto(proto);

  ASSERT_THAT(restored_key_bag, SizeIs(1));
  EXPECT_TRUE(restored_key_bag.HasKey(real_key_name));
  EXPECT_FALSE(restored_key_bag.HasKey(actual_key_name_in_proto));
}

}  // namespace
}  // namespace syncer
