// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/scopes/leveldb_scopes_coding.h"

#include <ostream>

#include "testing/gtest/include/gtest/gtest.h"

namespace leveldb {
std::ostream& operator<<(std::ostream& os, const leveldb::Slice& slice) {
  os << "0x";
  for (size_t i = 0; i < slice.size(); ++i) {
    const char c = slice[i];
    os << std::hex << std::setfill('0') << std::setw(2) << (int)c;
  }
  return os;
}
}  // namespace leveldb

namespace content {
namespace {

TEST(LevelDBScopesCodingTest, GlobalMetadataKey) {
  std::vector<uint8_t> scopes_prefix = {'A', 'B'};
  ScopesEncoder encoder;

  std::string expected = "AB";
  expected.push_back(0x00);
  EXPECT_EQ(leveldb::Slice(expected), encoder.GlobalMetadataKey(scopes_prefix));
}

TEST(LevelDBScopesCodingTest, ScopeMetadataKey) {
  const static int kScopeNumber = 0x0201;
  std::vector<uint8_t> scopes_prefix = {'A', 'B'};
  ScopesEncoder encoder;

  std::string expected = "AB\x01\x81\x04";
  EXPECT_EQ(leveldb::Slice(expected),
            encoder.ScopeMetadataKey(scopes_prefix, kScopeNumber));
}

TEST(LevelDBScopesCodingTest, ScopeMetadataPrefix) {
  std::vector<uint8_t> scopes_prefix = {'A', 'B'};
  ScopesEncoder encoder;

  std::string expected = "AB\x01";
  EXPECT_EQ(leveldb::Slice(expected),
            encoder.ScopeMetadataPrefix(scopes_prefix));
}

TEST(LevelDBScopesCodingTest, TasksKeyPrefix) {
  std::vector<uint8_t> scopes_prefix = {'A', 'B'};
  ScopesEncoder encoder;

  std::string expected = "AB\x02";
  EXPECT_EQ(leveldb::Slice(expected), encoder.TasksKeyPrefix(scopes_prefix));
}

TEST(LevelDBScopesCodingTest, TasksKeyWithScopePrefix) {
  const static int kScopeNumber = 0x0401;
  std::vector<uint8_t> scopes_prefix = {'A', 'B'};
  ScopesEncoder encoder;

  std::string expected = "AB\x02\x81\x08";
  EXPECT_EQ(leveldb::Slice(expected),
            encoder.TasksKeyPrefix(scopes_prefix, kScopeNumber));
}

TEST(LevelDBScopesCodingTest, UndoTaskKeyPrefix) {
  const static int kScopeNumber = 0x0401;
  std::vector<uint8_t> scopes_prefix = {'A', 'B'};
  ScopesEncoder encoder;

  std::string expected = "AB\x02\x81\x08";
  expected.push_back(0x00);
  EXPECT_EQ(leveldb::Slice(expected),
            encoder.UndoTaskKeyPrefix(scopes_prefix, kScopeNumber));
}

TEST(LevelDBScopesCodingTest, CleanupTaskKeyPrefix) {
  const static int kScopeNumber = 0x0401;
  std::vector<uint8_t> scopes_prefix = {'A', 'B'};
  ScopesEncoder encoder;

  std::string expected = "AB\x02\x81\x08\x01";
  EXPECT_EQ(leveldb::Slice(expected),
            encoder.CleanupTaskKeyPrefix(scopes_prefix, kScopeNumber));
}

TEST(LevelDBScopesCodingTest, UndoTaskKey) {
  const static int kScopeNumber = 0x0401;
  const static int kSequenceNumber = 0x0401;
  std::vector<uint8_t> scopes_prefix = {'A', 'B'};
  ScopesEncoder encoder;

  std::string expected = "AB\x02\x81\x08";
  // Add the 'undo' type.
  expected.push_back(0x00);
  // Encode the big endian fixed length nunber.
  for (size_t i = 0; i < 6; ++i)
    expected.push_back(0x00);
  expected += "\x04\x01";
  EXPECT_EQ(leveldb::Slice(expected),
            encoder.UndoTaskKey(scopes_prefix, kScopeNumber, kSequenceNumber));
}

TEST(LevelDBScopesCodingTest, CleanupTaskKey) {
  const static int kScopeNumber = 0x0401;
  const static int kSequenceNumber = 0x0401;
  std::vector<uint8_t> scopes_prefix = {'A', 'B'};
  ScopesEncoder encoder;

  std::string expected = "AB\x02\x81\x08\x01";
  // Encode the big endian fixed length nunber.
  for (size_t i = 0; i < 6; ++i)
    expected.push_back(0x00);
  expected += "\x04\x01";
  EXPECT_EQ(
      leveldb::Slice(expected),
      encoder.CleanupTaskKey(scopes_prefix, kScopeNumber, kSequenceNumber));
}

TEST(LevelDBScopesCodingTest, ParseScopeMetadataId) {
  const static int kScopeNumber = 0x0401;
  std::vector<uint8_t> scopes_prefix = {'A', 'B'};
  bool success = false;
  int64_t scope_id;

  std::string on_disk = "AB\x01\x81\x08";
  std::tie(success, scope_id) =
      leveldb_scopes::ParseScopeMetadataId(on_disk, scopes_prefix);
  EXPECT_TRUE(success);
  EXPECT_EQ(kScopeNumber, scope_id);
}

TEST(LevelDBScopesCodingTest, InvalidMetadataByte) {
  std::vector<uint8_t> scopes_prefix = {'A', 'B'};
  bool success = false;
  int64_t scope_id;

  // Wrong metadata byte.
  std::string on_disk = "AB\x02\x81\x08";
  std::tie(success, scope_id) =
      leveldb_scopes::ParseScopeMetadataId(on_disk, scopes_prefix);
  EXPECT_FALSE(success);
}

TEST(LevelDBScopesCodingTest, InvalidVarInt) {
  std::vector<uint8_t> scopes_prefix = {'A', 'B'};
  bool success = false;
  int64_t scope_id;

  // Invalid varint
  std::string on_disk = "AB\x01\xFF";
  std::tie(success, scope_id) =
      leveldb_scopes::ParseScopeMetadataId(on_disk, scopes_prefix);
  EXPECT_FALSE(success);
  on_disk = "AB\x01";
  std::tie(success, scope_id) =
      leveldb_scopes::ParseScopeMetadataId(on_disk, scopes_prefix);
  EXPECT_FALSE(success);
}

TEST(LevelDBScopesCodingTest, InvalidPrefix) {
  std::vector<uint8_t> scopes_prefix = {'A', 'B'};

  bool success = false;
  int64_t scope_id;
  // Invalid prefix
  std::string on_disk = "XX\x01\x81\x08";
  std::tie(success, scope_id) =
      leveldb_scopes::ParseScopeMetadataId(on_disk, scopes_prefix);
  EXPECT_FALSE(success);
  on_disk = "A";
  std::tie(success, scope_id) =
      leveldb_scopes::ParseScopeMetadataId(on_disk, scopes_prefix);
  EXPECT_FALSE(success);
}

}  // namespace
}  // namespace content
