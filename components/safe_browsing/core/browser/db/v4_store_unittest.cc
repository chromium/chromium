// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v4_store.h"

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "components/safe_browsing/core/browser/db/v4_store.pb.h"
#include "components/safe_browsing/core/common/features.h"
#include "crypto/sha2.h"
#include "testing/platform_test.h"

namespace safe_browsing {

using ::google::protobuf::int32;
using ::google::protobuf::RepeatedField;
using ::google::protobuf::RepeatedPtrField;

class InMemoryV4Store : public V4Store {
 public:
  InMemoryV4Store(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                  const base::FilePath& store_path)
      : V4Store(task_runner,
                store_path,
                std::make_unique<InMemoryHashPrefixMap>()) {}
};

class V4StoreTest : public PlatformTest {
 public:
  V4StoreTest() : task_runner_(new base::TestSimpleTaskRunner) {}

  void SetUp() override {
    PlatformTest::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    store_path_ = temp_dir_.GetPath().AppendASCII("V4StoreTest.store");
    DVLOG(1) << "store_path_: " << store_path_.value();
  }

  void TearDown() override {
    base::DeleteFile(store_path_);
    PlatformTest::TearDown();
  }

  void WriteFileFormatProtoToFile(uint32_t magic,
                                  uint32_t version = 0,
                                  ListUpdateResponse* response = nullptr) {
    V4StoreFileFormat file_format;
    WriteFileFormatProtoToFile(&file_format, magic, version, response);
  }

  void WriteFileFormatProtoToFile(V4StoreFileFormat* file_format,
                                  uint32_t magic,
                                  uint32_t version,
                                  ListUpdateResponse* response) {
    file_format->set_magic_number(magic);
    file_format->set_version_number(version);
    if (response != nullptr) {
      ListUpdateResponse* list_update_response =
          file_format->mutable_list_update_response();
      *list_update_response = *response;
    }

    std::string file_format_string;
    file_format->SerializeToString(&file_format_string);
    base::WriteFile(store_path_, file_format_string);
  }

  void UpdatedStoreReady(bool* called_back,
                         bool expect_store,
                         std::unique_ptr<V4Store> store) {
    *called_back = true;
    if (expect_store) {
      ASSERT_TRUE(store);
      EXPECT_EQ(2u, store->hash_prefix_map_->view().size());
      EXPECT_EQ("22222", store->hash_prefix_map_->view()[5]);
      EXPECT_EQ("abcd", store->hash_prefix_map_->view()[4]);
    } else {
      ASSERT_FALSE(store);
    }

    updated_store_ = std::move(store);
  }

  base::Time GetLastModifiedTime(const base::FilePath& path) {
    base::File::Info info;
    base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    EXPECT_TRUE(file.GetInfo(&info));
    return info.last_modified;
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath store_path_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<V4Store> updated_store_;
};

TEST_F(V4StoreTest, TestReadFromEmptyFile) {
  base::CloseFile(base::OpenFile(store_path_, "wb+"));

  InMemoryV4Store store(task_runner_, store_path_);
  EXPECT_EQ(FILE_EMPTY_FAILURE, store.ReadFromDisk());
  EXPECT_FALSE(store.HasValidData());
}

TEST_F(V4StoreTest, TestReadFromAbsentFile) {
  EXPECT_EQ(FILE_UNREADABLE_FAILURE,
            InMemoryV4Store(task_runner_, store_path_).ReadFromDisk());
}

TEST_F(V4StoreTest, TestReadFromInvalidContentsFile) {
  const char kInvalidContents[] = "Chromium";
  base::WriteFile(store_path_, kInvalidContents);
  EXPECT_EQ(PROTO_PARSING_FAILURE,
            InMemoryV4Store(task_runner_, store_path_).ReadFromDisk());
}

TEST_F(V4StoreTest, TestReadFromFileWithUnknownProto) {
  Checksum checksum;
  checksum.set_sha256("checksum");
  std::string checksum_string;
  checksum.SerializeToString(&checksum_string);
  base::WriteFile(store_path_, checksum_string);

  // Even though we wrote a completely different proto to file, the proto
  // parsing method does not fail. This shows the importance of a magic number.
  EXPECT_EQ(UNEXPECTED_MAGIC_NUMBER_FAILURE,
            InMemoryV4Store(task_runner_, store_path_).ReadFromDisk());
}

TEST_F(V4StoreTest, TestReadFromUnexpectedMagicFile) {
  WriteFileFormatProtoToFile(111);
  EXPECT_EQ(UNEXPECTED_MAGIC_NUMBER_FAILURE,
            InMemoryV4Store(task_runner_, store_path_).ReadFromDisk());
}

TEST_F(V4StoreTest, TestReadFromLowVersionFile) {
  WriteFileFormatProtoToFile(0x600D71FE, 2);
  EXPECT_EQ(FILE_VERSION_INCOMPATIBLE_FAILURE,
            InMemoryV4Store(task_runner_, store_path_).ReadFromDisk());
}

TEST_F(V4StoreTest, TestReadFromNoHashPrefixInfoFile) {
  WriteFileFormatProtoToFile(0x600D71FE, 9);
  EXPECT_EQ(HASH_PREFIX_INFO_MISSING_FAILURE,
            InMemoryV4Store(task_runner_, store_path_).ReadFromDisk());
}

TEST_F(V4StoreTest, TestReadFromNoHashPrefixesFile) {
  ListUpdateResponse list_update_response;
  list_update_response.set_platform_type(LINUX_PLATFORM);
  list_update_response.set_response_type(ListUpdateResponse::FULL_UPDATE);
  WriteFileFormatProtoToFile(0x600D71FE, 9, &list_update_response);
  InMemoryV4Store store(task_runner_, store_path_);
  EXPECT_EQ(READ_SUCCESS, store.ReadFromDisk());
  EXPECT_TRUE(store.hash_prefix_map_->view().empty());
  EXPECT_EQ(14, store.file_size_);
  EXPECT_FALSE(store.HasValidData());
}

TEST_F(V4StoreTest, TestAddUnlumpedHashesWithInvalidAddition) {
  InMemoryHashPrefixMap prefix_map;
  EXPECT_EQ(ADDITIONS_SIZE_UNEXPECTED_FAILURE,
            V4Store::AddUnlumpedHashes(5, "a", &prefix_map));
  EXPECT_TRUE(prefix_map.view().empty());
}

TEST_F(V4StoreTest, TestAddUnlumpedHashesWithEmptyString) {
  InMemoryHashPrefixMap prefix_map;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "", &prefix_map));
  EXPECT_TRUE(prefix_map.view()[5].empty());
}

TEST_F(V4StoreTest, TestAddUnlumpedHashes) {
  InMemoryHashPrefixMap prefix_map;
  PrefixSize prefix_size = 5;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(prefix_size, "abcde5432100000-----",
                                       &prefix_map));
  EXPECT_EQ(1u, prefix_map.view().size());
  HashPrefixesView hash_prefixes = prefix_map.view().at(prefix_size);
  EXPECT_EQ(4 * prefix_size, hash_prefixes.size());
  EXPECT_EQ("abcde5432100000-----", hash_prefixes);

  prefix_size = 4;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(prefix_size, "abcde5432100000-----",
                                       &prefix_map));
  EXPECT_EQ(2u, prefix_map.view().size());
  hash_prefixes = prefix_map.view().at(prefix_size);
  EXPECT_EQ(5 * prefix_size, hash_prefixes.size());
  EXPECT_EQ("abcde5432100000-----", hash_prefixes);
}

TEST_F(V4StoreTest, TestGetNextSmallestUnmergedPrefixWithEmptyPrefixMap) {
  InMemoryHashPrefixMap prefix_map;
  IteratorMap iterator_map;
  V4Store::InitializeIteratorMap(prefix_map, &iterator_map);

  HashPrefixStr prefix;
  EXPECT_FALSE(V4Store::GetNextSmallestUnmergedPrefix(prefix_map, iterator_map,
                                                      &prefix));
}

TEST_F(V4StoreTest, TestGetNextSmallestUnmergedPrefix) {
  InMemoryHashPrefixMap prefix_map;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "-----0000054321abcde", &prefix_map));
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "*****0000054321abcde", &prefix_map));
  IteratorMap iterator_map;
  V4Store::InitializeIteratorMap(prefix_map, &iterator_map);

  HashPrefixStr prefix;
  EXPECT_TRUE(V4Store::GetNextSmallestUnmergedPrefix(prefix_map, iterator_map,
                                                     &prefix));
  EXPECT_EQ("****", prefix);
}

TEST_F(V4StoreTest, TestMergeUpdatesWithSameSizesInEachMap) {
  InMemoryHashPrefixMap prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "abcdefgh", &prefix_map_old));
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "54321abcde", &prefix_map_old));
  InMemoryHashPrefixMap prefix_map_additions;
  EXPECT_EQ(
      APPLY_UPDATE_SUCCESS,
      V4Store::AddUnlumpedHashes(4, "----1111bbbb", &prefix_map_additions));
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "22222bcdef", &prefix_map_additions));

  InMemoryV4Store store(task_runner_, store_path_);
  // Proof of checksum validity using python:
  // >>> import hashlib
  // >>> m = hashlib.sha256()
  // >>> m.update("----11112222254321abcdabcdebbbbbcdefefgh")
  // >>> m.digest()
  // "\xbc\xb3\xedk\xe3x\xd1(\xa9\xedz7]"
  // "x\x18\xbdn]\xa5\xa8R\xf7\xab\xcf\xc1\xa3\xa3\xc5Z,\xa6o"
  std::string expected_checksum = std::string(
      "\xBC\xB3\xEDk\xE3x\xD1(\xA9\xEDz7]x\x18\xBDn]"
      "\xA5\xA8R\xF7\xAB\xCF\xC1\xA3\xA3\xC5Z,\xA6o",
      crypto::kSHA256Length);
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            store.MergeUpdate(prefix_map_old, prefix_map_additions, nullptr,
                              expected_checksum));

  HashPrefixMapView prefix_map = store.hash_prefix_map_->view();
  EXPECT_EQ(2u, prefix_map.size());

  PrefixSize prefix_size = 4;
  HashPrefixesView hash_prefixes = prefix_map.at(prefix_size);
  EXPECT_EQ(5 * prefix_size, hash_prefixes.size());
  EXPECT_EQ("----", hash_prefixes.substr(0 * prefix_size, prefix_size));
  EXPECT_EQ("1111", hash_prefixes.substr(1 * prefix_size, prefix_size));
  EXPECT_EQ("abcd", hash_prefixes.substr(2 * prefix_size, prefix_size));
  EXPECT_EQ("bbbb", hash_prefixes.substr(3 * prefix_size, prefix_size));
  EXPECT_EQ("efgh", hash_prefixes.substr(4 * prefix_size, prefix_size));

  prefix_size = 5;
  hash_prefixes = prefix_map.at(prefix_size);
  EXPECT_EQ(4 * prefix_size, hash_prefixes.size());
  EXPECT_EQ("22222", hash_prefixes.substr(0 * prefix_size, prefix_size));
  EXPECT_EQ("54321", hash_prefixes.substr(1 * prefix_size, prefix_size));
  EXPECT_EQ("abcde", hash_prefixes.substr(2 * prefix_size, prefix_size));
  EXPECT_EQ("bcdef", hash_prefixes.substr(3 * prefix_size, prefix_size));
}

TEST_F(V4StoreTest, TestMergeUpdatesWithDifferentSizesInEachMap) {
  InMemoryHashPrefixMap prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "1111abcdefgh", &prefix_map_old));
  InMemoryHashPrefixMap prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "22222bcdef", &prefix_map_additions));

  InMemoryV4Store store(task_runner_, store_path_);
  std::string expected_checksum = std::string(
      "\xA5\x8B\xCAsD\xC7\xF9\xCE\xD2\xF4\x4="
      "\xB2\"\x82\x1A\xC1\xB8\x1F\x10\r\v\x9A\x93\xFD\xE1\xB8"
      "B\x1Eh\xF7\xB4",
      crypto::kSHA256Length);
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            store.MergeUpdate(prefix_map_old, prefix_map_additions, nullptr,
                              expected_checksum));

  HashPrefixMapView prefix_map = store.hash_prefix_map_->view();
  EXPECT_EQ(2u, prefix_map.size());

  PrefixSize prefix_size = 4;
  HashPrefixesView hash_prefixes = prefix_map.at(prefix_size);
  EXPECT_EQ(3 * prefix_size, hash_prefixes.size());
  EXPECT_EQ("1111abcdefgh", hash_prefixes);

  prefix_size = 5;
  hash_prefixes = prefix_map.at(prefix_size);
  EXPECT_EQ(2 * prefix_size, hash_prefixes.size());
  EXPECT_EQ("22222bcdef", hash_prefixes);
}

TEST_F(V4StoreTest, TestMergeUpdatesOldMapRunsOutFirst) {
  InMemoryHashPrefixMap prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "00001111", &prefix_map_old));
  InMemoryHashPrefixMap prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "2222", &prefix_map_additions));

  InMemoryV4Store store(task_runner_, store_path_);
  std::string expected_checksum = std::string(
      "\x84\x92\xET\xED\xF7\x97"
      "C\xCE}\xFF"
      "E\x1\xAB-\b>\xDB\x95\b\xD8H\xD5\x1D\xF9]8x\xA4\xD4\xC2\xFA",
      crypto::kSHA256Length);
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            store.MergeUpdate(prefix_map_old, prefix_map_additions, nullptr,
                              expected_checksum));

  HashPrefixMapView prefix_map = store.hash_prefix_map_->view();
  EXPECT_EQ(1u, prefix_map.size());

  PrefixSize prefix_size = 4;
  HashPrefixesView hash_prefixes = prefix_map.at(prefix_size);
  EXPECT_EQ(3 * prefix_size, hash_prefixes.size());
  EXPECT_EQ("0000", hash_prefixes.substr(0 * prefix_size, prefix_size));
  EXPECT_EQ("1111", hash_prefixes.substr(1 * prefix_size, prefix_size));
  EXPECT_EQ("2222", hash_prefixes.substr(2 * prefix_size, prefix_size));
}

TEST_F(V4StoreTest, TestMergeUpdatesAdditionsMapRunsOutFirst) {
  InMemoryHashPrefixMap prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "2222", &prefix_map_old));
  InMemoryHashPrefixMap prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "00001111", &prefix_map_additions));

  InMemoryV4Store store(task_runner_, store_path_);
  std::string expected_checksum = std::string(
      "\x84\x92\xET\xED\xF7\x97"
      "C\xCE}\xFF"
      "E\x1\xAB-\b>\xDB\x95\b\xD8H\xD5\x1D\xF9]8x\xA4\xD4\xC2\xFA",
      crypto::kSHA256Length);
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            store.MergeUpdate(prefix_map_old, prefix_map_additions, nullptr,
                              expected_checksum));

  HashPrefixMapView prefix_map = store.hash_prefix_map_->view();
  EXPECT_EQ(1u, prefix_map.size());

  PrefixSize prefix_size = 4;
  HashPrefixesView hash_prefixes = prefix_map.at(prefix_size);
  EXPECT_EQ(3 * prefix_size, hash_prefixes.size());
  EXPECT_EQ("0000", hash_prefixes.substr(0 * prefix_size, prefix_size));
  EXPECT_EQ("1111", hash_prefixes.substr(1 * prefix_size, prefix_size));
  EXPECT_EQ("2222", hash_prefixes.substr(2 * prefix_size, prefix_size));
}

TEST_F(V4StoreTest, TestMergeUpdatesFailsForRepeatedHashPrefix) {
  InMemoryHashPrefixMap prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "2222", &prefix_map_old));
  InMemoryHashPrefixMap prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "2222", &prefix_map_additions));

  InMemoryV4Store store(task_runner_, store_path_);
  std::string expected_checksum;
  EXPECT_EQ(ADDITIONS_HAS_EXISTING_PREFIX_FAILURE,
            store.MergeUpdate(prefix_map_old, prefix_map_additions, nullptr,
                              expected_checksum));
}

TEST_F(V4StoreTest, TestMergeUpdatesFailsWhenRemovalsIndexTooLarge) {
  InMemoryHashPrefixMap prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "2222", &prefix_map_old));
  InMemoryHashPrefixMap prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "11113333", &prefix_map_additions));

  // Even though the merged map could have size 3 without removals, the
  // removals index should only count the entries in the old map.
  InMemoryV4Store store(task_runner_, store_path_);
  RepeatedField<int32> raw_removals;
  // old_store: ["2222"]
  raw_removals.Add(1);
  std::string expected_checksum;
  EXPECT_EQ(REMOVALS_INDEX_TOO_LARGE_FAILURE,
            store.MergeUpdate(prefix_map_old, prefix_map_additions,
                              &raw_removals, expected_checksum));
}

TEST_F(V4StoreTest, TestMergeUpdatesRemovesOnlyElement) {
  InMemoryHashPrefixMap prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "2222", &prefix_map_old));
  InMemoryHashPrefixMap prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "1111133333", &prefix_map_additions));

  InMemoryV4Store store(task_runner_, store_path_);
  RepeatedField<int32> raw_removals;
  // old_store: ["2222"]
  raw_removals.Add(0);  // Removes "2222"
  std::string expected_checksum = std::string(
      "\xE6\xB0\x1\x12\x89\x83\xF0/"
      "\xE7\xD2\xE6\xDC\x16\xB9\x8C+\xA2\xB3\x9E\x89<,\x88"
      "B3\xA5\xB1"
      "D\x9E\x9E'\x14",
      crypto::kSHA256Length);
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            store.MergeUpdate(prefix_map_old, prefix_map_additions,
                              &raw_removals, expected_checksum));

  HashPrefixMapView prefix_map = store.hash_prefix_map_->view();
  // The size is 2 since we reserve space anyway.
  EXPECT_EQ(2u, prefix_map.size());
  EXPECT_TRUE(prefix_map.at(4).empty());
  EXPECT_EQ("1111133333", prefix_map.at(5));
}

TEST_F(V4StoreTest, TestMergeUpdatesRemovesFirstElement) {
  InMemoryHashPrefixMap prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "22224444", &prefix_map_old));
  InMemoryHashPrefixMap prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "1111133333", &prefix_map_additions));

  InMemoryV4Store store(task_runner_, store_path_);
  RepeatedField<int32> raw_removals;
  // old_store: ["2222", "4444"]
  raw_removals.Add(0);  // Removes "2222"
  std::string expected_checksum = std::string(
      "\x9D\xF3\xF2\x82\0\x1E{\xDF\xCD\xC0V\xBE\xD6<\x85"
      "D7=\xB5v\xAD\b1\xC9\xB3"
      "A\xAC"
      "b\xF1lf\xA4",
      crypto::kSHA256Length);
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            store.MergeUpdate(prefix_map_old, prefix_map_additions,
                              &raw_removals, expected_checksum));

  HashPrefixMapView prefix_map = store.hash_prefix_map_->view();
  // The size is 2 since we reserve space anyway.
  EXPECT_EQ(2u, prefix_map.size());
  EXPECT_EQ("4444", prefix_map.at(4));
  EXPECT_EQ("1111133333", prefix_map.at(5));
}

TEST_F(V4StoreTest, TestMergeUpdatesRemovesMiddleElement) {
  InMemoryHashPrefixMap prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "222233334444", &prefix_map_old));
  InMemoryHashPrefixMap prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "1111133333", &prefix_map_additions));

  InMemoryV4Store store(task_runner_, store_path_);
  RepeatedField<int32> raw_removals;
  // old_store: ["2222", "3333", 4444"]
  raw_removals.Add(1);  // Removes "3333"
  std::string expected_checksum = std::string(
      "\xFA-A\x15{\x17\0>\xAE"
      "8\xACigR\xD1\x93<\xB2\xC9\xB5\x81\xC0\xFB\xBB\x2\f\xAFpN\xEA"
      "44",
      crypto::kSHA256Length);
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            store.MergeUpdate(prefix_map_old, prefix_map_additions,
                              &raw_removals, expected_checksum));

  HashPrefixMapView prefix_map = store.hash_prefix_map_->view();
  // The size is 2 since we reserve space anyway.
  EXPECT_EQ(2u, prefix_map.size());
  EXPECT_EQ("22224444", prefix_map.at(4));
  EXPECT_EQ("1111133333", prefix_map.at(5));
}

TEST_F(V4StoreTest, TestMergeUpdatesRemovesLastElement) {
  InMemoryHashPrefixMap prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "222233334444", &prefix_map_old));
  InMemoryHashPrefixMap prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "1111133333", &prefix_map_additions));

  InMemoryV4Store store(task_runner_, store_path_);
  RepeatedField<int32> raw_removals;
  // old_store: ["2222", "3333", 4444"]
  raw_removals.Add(2);  // Removes "4444"
  std::string expected_checksum = std::string(
      "a\xE1\xAD\x96\xFE\xA6"
      "A\xCA~7W\xF6z\xD8\n\xCA?\x96\x8A\x17U\x5\v\r\x88]\n\xB2JX\xC4S",
      crypto::kSHA256Length);
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            store.MergeUpdate(prefix_map_old, prefix_map_additions,
                              &raw_removals, expected_checksum));

  HashPrefixMapView prefix_map = store.hash_prefix_map_->view();
  // The size is 2 since we reserve space anyway.
  EXPECT_EQ(2u, prefix_map.size());
  EXPECT_EQ("22223333", prefix_map.at(4));
  EXPECT_EQ("1111133333", prefix_map.at(5));
}

TEST_F(V4StoreTest, TestMergeUpdatesRemovesWhenOldHasDifferentSizes) {
  InMemoryHashPrefixMap prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "222233334444", &prefix_map_old));
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "aaaaabbbbb", &prefix_map_old));
  InMemoryHashPrefixMap prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "1111133333", &prefix_map_additions));

  InMemoryV4Store store(task_runner_, store_path_);
  RepeatedField<int32> raw_removals;
  // old_store: ["2222", "3333", 4444", "aaaaa", "bbbbb"]
  raw_removals.Add(3);  // Removes "aaaaa"
  std::string expected_checksum = std::string(
      "\xA7OG\x9D\x83.\x9D-f\x8A\xE\x8B\r&\x19"
      "6\xE3\xF0\xEFTi\xA7\x5\xEA\xF7"
      "ej,\xA8\x9D\xAD\x91",
      crypto::kSHA256Length);
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            store.MergeUpdate(prefix_map_old, prefix_map_additions,
                              &raw_removals, expected_checksum));

  HashPrefixMapView prefix_map = store.hash_prefix_map_->view();
  // The size is 2 since we reserve space anyway.
  EXPECT_EQ(2u, prefix_map.size());
  EXPECT_EQ("222233334444", prefix_map.at(4));
  EXPECT_EQ("1111133333bbbbb", prefix_map.at(5));
}

TEST_F(V4StoreTest, TestMergeUpdatesRemovesMultipleAcrossDifferentSizes) {
  InMemoryHashPrefixMap prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "22223333aaaa", &prefix_map_old));
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "3333344444bbbbb", &prefix_map_old));
  InMemoryHashPrefixMap prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "11111", &prefix_map_additions));

  InMemoryV4Store store(task_runner_, store_path_);
  RepeatedField<int32> raw_removals;
  // old_store: ["2222", "3333", "33333", "44444", "aaaa", "bbbbb"]
  raw_removals.Add(1);  // Removes "3333"
  raw_removals.Add(3);  // Removes "44444"
  std::string expected_checksum = std::string(
      "!D\xB7&L\xA7&G0\x85\xB4"
      "E\xDD\x10\"\x9A\xCA\xF1"
      "3^\x83w\xBBL\x19n\xAD\xBDM\x9D"
      "b\x9F",
      crypto::kSHA256Length);
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            store.MergeUpdate(prefix_map_old, prefix_map_additions,
                              &raw_removals, expected_checksum));

  HashPrefixMapView prefix_map = store.hash_prefix_map_->view();
  // The size is 2 since we reserve space anyway.
  EXPECT_EQ(2u, prefix_map.size());
  EXPECT_EQ("2222aaaa", prefix_map.at(4));
  EXPECT_EQ("1111133333bbbbb", prefix_map.at(5));
}

TEST_F(V4StoreTest, TestReadFullResponseWithValidHashPrefixMap) {
  InMemoryV4Store write_store(task_runner_, store_path_);
  write_store.hash_prefix_map_->Append(4, "00000abc");
  write_store.hash_prefix_map_->Append(5, "00000abcde");
  write_store.state_ = "test_client_state";
  EXPECT_FALSE(base::PathExists(write_store.store_path_));
  EXPECT_EQ(WRITE_SUCCESS, write_store.WriteToDisk(Checksum()));
  EXPECT_TRUE(base::PathExists(write_store.store_path_));

  InMemoryV4Store read_store(task_runner_, store_path_);
  EXPECT_EQ(READ_SUCCESS, read_store.ReadFromDisk());
  EXPECT_EQ("test_client_state", read_store.state_);
  ASSERT_EQ(2u, read_store.hash_prefix_map_->view().size());
  EXPECT_EQ("00000abc", read_store.hash_prefix_map_->view()[4]);
  EXPECT_EQ("00000abcde", read_store.hash_prefix_map_->view()[5]);
  EXPECT_EQ(71, read_store.file_size_);
}

// This tests fails to read the prefix map from the disk because the file on
// disk is invalid. The hash prefixes string is 6 bytes long, but the prefix
// size is 5 so the parser isn't able to split the hash prefixes list
// completely.
TEST_F(V4StoreTest, TestReadFullResponseWithInvalidHashPrefixMap) {
  InMemoryV4Store write_store(task_runner_, store_path_);
  write_store.hash_prefix_map_->Append(5, "abcdef");
  write_store.state_ = "test_client_state";
  EXPECT_FALSE(base::PathExists(write_store.store_path_));
  EXPECT_EQ(WRITE_SUCCESS, write_store.WriteToDisk(Checksum()));
  EXPECT_TRUE(base::PathExists(write_store.store_path_));

  InMemoryV4Store read_store(task_runner_, store_path_);
  EXPECT_EQ(HASH_PREFIX_MAP_GENERATION_FAILURE, read_store.ReadFromDisk());
  EXPECT_TRUE(read_store.state_.empty());
  EXPECT_TRUE(read_store.hash_prefix_map_->view().empty());
  EXPECT_EQ(0, read_store.file_size_);
}

TEST_F(V4StoreTest, TestHashPrefixExistsAtTheBeginning) {
  InMemoryHashPrefixMap map;
  map.Append(5, "abcdebbbbbccccc");
  HashPrefixStr hash_prefix = "abcde";
  EXPECT_EQ(map.GetMatchingHashPrefix(hash_prefix), hash_prefix);
}

TEST_F(V4StoreTest, TestHashPrefixExistsInTheMiddle) {
  InMemoryHashPrefixMap map;
  map.Append(5, "abcdebbbbbccccc");
  HashPrefixStr hash_prefix = "bbbbb";
  EXPECT_EQ(map.GetMatchingHashPrefix(hash_prefix), hash_prefix);
}

TEST_F(V4StoreTest, TestHashPrefixExistsAtTheEnd) {
  InMemoryHashPrefixMap map;
  map.Append(5, "abcdebbbbbccccc");
  HashPrefixStr hash_prefix = "ccccc";
  EXPECT_EQ(map.GetMatchingHashPrefix(hash_prefix), hash_prefix);
}

TEST_F(V4StoreTest, TestHashPrefixExistsAtTheBeginningOfEven) {
  InMemoryHashPrefixMap map;
  map.Append(5, "abcdebbbbb");
  HashPrefixStr hash_prefix = "abcde";
  EXPECT_EQ(map.GetMatchingHashPrefix(hash_prefix), hash_prefix);
}

TEST_F(V4StoreTest, TestHashPrefixExistsAtTheEndOfEven) {
  InMemoryHashPrefixMap map;
  map.Append(5, "abcdebbbbb");
  HashPrefixStr hash_prefix = "bbbbb";
  EXPECT_EQ(map.GetMatchingHashPrefix(hash_prefix), hash_prefix);
}

TEST_F(V4StoreTest, TestHashPrefixDoesNotExistInConcatenatedList) {
  InMemoryHashPrefixMap map;
  map.Append(5, "abcdebbbbb");
  HashPrefixStr hash_prefix = "bbbbc";
  EXPECT_EQ(map.GetMatchingHashPrefix(hash_prefix), "");
}

TEST_F(V4StoreTest, TestFullHashExistsInMapWithSingleSize) {
  InMemoryV4Store store(task_runner_, store_path_);
  store.hash_prefix_map_->Append(
      32, "0111222233334444555566667777888811112222333344445555666677778888");
  FullHashStr full_hash = "11112222333344445555666677778888";
  EXPECT_EQ("11112222333344445555666677778888",
            store.GetMatchingHashPrefix(full_hash));
}

TEST_F(V4StoreTest, TestFullHashExistsInMapWithDifferentSizes) {
  InMemoryV4Store store(task_runner_, store_path_);
  store.hash_prefix_map_->Append(4, "22223333aaaa");
  store.hash_prefix_map_->Append(32, "11112222333344445555666677778888");
  FullHashStr full_hash = "11112222333344445555666677778888";
  EXPECT_EQ("11112222333344445555666677778888",
            store.GetMatchingHashPrefix(full_hash));
}

TEST_F(V4StoreTest, TestHashPrefixExistsInMapWithSingleSize) {
  InMemoryV4Store store(task_runner_, store_path_);
  store.hash_prefix_map_->Append(4, "22223333aaaa");
  FullHashStr full_hash = "22222222222222222222222222222222";
  EXPECT_EQ("2222", store.GetMatchingHashPrefix(full_hash));
}

TEST_F(V4StoreTest, TestHashPrefixExistsInMapWithDifferentSizes) {
  InMemoryV4Store store(task_runner_, store_path_);
  store.hash_prefix_map_->Append(4, "22223333aaaa");
  store.hash_prefix_map_->Append(5, "11111hhhhh");
  FullHashStr full_hash = "22222222222222222222222222222222";
  EXPECT_EQ("2222", store.GetMatchingHashPrefix(full_hash));
}

TEST_F(V4StoreTest, TestHashPrefixDoesNotExistInMapWithDifferentSizes) {
  InMemoryV4Store store(task_runner_, store_path_);
  store.hash_prefix_map_->Append(4, "3333aaaa");
  store.hash_prefix_map_->Append(5, "11111hhhhh");
  FullHashStr full_hash = "22222222222222222222222222222222";
  EXPECT_TRUE(store.GetMatchingHashPrefix(full_hash).empty());
}

TEST_F(V4StoreTest, GetMatchingHashPrefixSize32Or21) {
  HashPrefixStr prefix = "0123";
  InMemoryV4Store store(task_runner_, store_path_);
  store.hash_prefix_map_->Append(4, prefix);

  FullHashStr full_hash_21 = "0123456789ABCDEF01234";
  EXPECT_EQ(prefix, store.GetMatchingHashPrefix(full_hash_21));
  FullHashStr full_hash_32 = "0123456789ABCDEF0123456789ABCDEF";
  EXPECT_EQ(prefix, store.GetMatchingHashPrefix(full_hash_32));
#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
  // This hits a DCHECK so it is release mode only.
  FullHashStr full_hash_22 = "0123456789ABCDEF012345";
  EXPECT_EQ(prefix, store.GetMatchingHashPrefix(full_hash_22));
#endif
}

#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
// This test hits a NOTREACHED so it is a release mode only test.
TEST_F(V4StoreTest, TestAdditionsWithRiceEncodingFailsWithInvalidInput) {
  RepeatedPtrField<ThreatEntrySet> additions;
  ThreatEntrySet* addition = additions.Add();
  addition->set_compression_type(RICE);
  addition->mutable_rice_hashes()->set_num_entries(-1);
  InMemoryHashPrefixMap additions_map;
  EXPECT_EQ(RICE_DECODING_FAILURE,
            InMemoryV4Store(task_runner_, store_path_)
                .UpdateHashPrefixMapFromAdditions("V4Metric", additions,
                                                  &additions_map));
}
#endif

TEST_F(V4StoreTest, TestAdditionsWithRiceEncodingSucceeds) {
  RepeatedPtrField<ThreatEntrySet> additions;
  ThreatEntrySet* addition = additions.Add();
  addition->set_compression_type(RICE);
  RiceDeltaEncoding* rice_hashes = addition->mutable_rice_hashes();
  rice_hashes->set_first_value(5);
  rice_hashes->set_num_entries(3);
  rice_hashes->set_rice_parameter(28);
  // The following value is hand-crafted by getting inspiration from:
  // https://goto.google.com/testlargenumbersriceencoded
  // The value listed at that place fails the "integer overflow" check so I
  // modified it until the decoder parsed it successfully.
  rice_hashes->set_encoded_data(
      "\xbf\xa8\x3f\xfb\xf\xf\x5e\x27\xe6\xc3\x1d\xc6\x38");
  InMemoryHashPrefixMap additions_map;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            InMemoryV4Store(task_runner_, store_path_)
                .UpdateHashPrefixMapFromAdditions("V4Metric", additions,
                                                  &additions_map));
  EXPECT_EQ(1u, additions_map.view().size());
  EXPECT_EQ(std::string("\x5\0\0\0\fL\x93\xADV\x7F\xF6o\xCEo1\x81", 16),
            additions_map.view()[4]);
}

TEST_F(V4StoreTest, TestRemovalsWithRiceEncodingSucceeds) {
  InMemoryHashPrefixMap prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "1111abcdefgh", &prefix_map_old));
  InMemoryHashPrefixMap prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "22222bcdef", &prefix_map_additions));

  InMemoryV4Store store(task_runner_, store_path_);
  std::string expected_checksum = std::string(
      "\xA5\x8B\xCAsD\xC7\xF9\xCE\xD2\xF4\x4="
      "\xB2\"\x82\x1A\xC1\xB8\x1F\x10\r\v\x9A\x93\xFD\xE1\xB8"
      "B\x1Eh\xF7\xB4",
      crypto::kSHA256Length);
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            store.MergeUpdate(prefix_map_old, prefix_map_additions, nullptr,
                              expected_checksum));
  EXPECT_FALSE(store.HasValidData());  // Never actually read from disk.

  // At this point, the store map looks like this:
  // 4: 1111abcdefgh
  // 5: 22222bcdef
  // sorted: 1111, 22222, abcd, bcdef, efgh
  // We'll now try to delete hashes at indexes 0, 3 and 4 in the sorted list.

  std::unique_ptr<ListUpdateResponse> lur(new ListUpdateResponse);
  lur->set_response_type(ListUpdateResponse::PARTIAL_UPDATE);
  ThreatEntrySet* removal = lur->add_removals();
  removal->set_compression_type(RICE);
  RiceDeltaEncoding* rice_indices = removal->mutable_rice_indices();
  rice_indices->set_first_value(0);
  rice_indices->set_num_entries(2);
  rice_indices->set_rice_parameter(2);
  rice_indices->set_encoded_data("\x16");

  bool called_back = false;
  UpdatedStoreReadyCallback store_ready_callback =
      base::BindOnce(&V4StoreTest::UpdatedStoreReady, base::Unretained(this),
                     &called_back, true /* expect_store */);
  EXPECT_FALSE(base::PathExists(store.store_path_));
  store.ApplyUpdate(std::move(lur), task_runner_,
                    std::move(store_ready_callback));
  EXPECT_TRUE(base::PathExists(store.store_path_));

  task_runner_->RunPendingTasks();
  base::RunLoop().RunUntilIdle();

  // This ensures that the callback was called.
  EXPECT_TRUE(called_back);
  // ApplyUpdate was successful, so we have valid data.
  ASSERT_TRUE(updated_store_);
  EXPECT_TRUE(updated_store_->HasValidData());
}

TEST_F(V4StoreTest, TestMergeUpdatesFailsChecksum) {
  // Proof of checksum mismatch using python:
  // >>> import hashlib
  // >>> m = hashlib.sha256()
  // >>> m.update("2222")
  // >>> m.digest()
  // "\xed\xee)\xf8\x82T;\x95f
  // \xb2m\x0e\xe0\xe7\xe9P9\x9b\x1cB"\xf5\xde\x05\xe0d%\xb4\xc9\x95\xe9"

  InMemoryHashPrefixMap prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "2222", &prefix_map_old));
  EXPECT_EQ(CHECKSUM_MISMATCH_FAILURE,
            InMemoryV4Store(task_runner_, store_path_)
                .MergeUpdate(prefix_map_old, InMemoryHashPrefixMap(), nullptr,
                             "aawc"));
}

TEST_F(V4StoreTest, TestChecksumErrorOnStartup) {
  // First the case of checksum not matching after reading from disk.
  ListUpdateResponse list_update_response;
  list_update_response.set_new_client_state("test_client_state");
  list_update_response.set_platform_type(LINUX_PLATFORM);
  list_update_response.set_response_type(ListUpdateResponse::FULL_UPDATE);
  list_update_response.mutable_checksum()->set_sha256(
      std::string(crypto::kSHA256Length, 0));
  WriteFileFormatProtoToFile(0x600D71FE, 9, &list_update_response);
  InMemoryV4Store store(task_runner_, store_path_);
  EXPECT_TRUE(store.expected_checksum_.empty());
  EXPECT_EQ(READ_SUCCESS, store.ReadFromDisk());
  EXPECT_TRUE(!store.expected_checksum_.empty());
  EXPECT_EQ(69, store.file_size_);
  EXPECT_EQ("test_client_state", store.state());

  EXPECT_FALSE(store.VerifyChecksum());

  // Now the case of checksum matching after reading from disk.
  // Proof of checksum mismatch using python:
  // >>> import hashlib
  // >>> m = hashlib.sha256()
  // >>> m.update("abcde")
  // >>> import base64
  // >>> encoded = base64.b64encode(m.digest())
  // >>> encoded
  // 'NrvlDtloQdEEQ7y2cNZVTwo0t2G+Z+ycSorSwMRMpCw='
  std::string expected_checksum;
  base::Base64Decode("NrvlDtloQdEEQ7y2cNZVTwo0t2G+Z+ycSorSwMRMpCw=",
                     &expected_checksum);
  ThreatEntrySet* additions = list_update_response.add_additions();
  additions->set_compression_type(RAW);
  additions->mutable_raw_hashes()->set_prefix_size(5);
  additions->mutable_raw_hashes()->set_raw_hashes("abcde");
  list_update_response.mutable_checksum()->set_sha256(expected_checksum);
  WriteFileFormatProtoToFile(0x600D71FE, 9, &list_update_response);
  InMemoryV4Store another_store(task_runner_, store_path_);
  EXPECT_TRUE(another_store.expected_checksum_.empty());

  EXPECT_EQ(READ_SUCCESS, another_store.ReadFromDisk());
  EXPECT_TRUE(!another_store.expected_checksum_.empty());
  EXPECT_EQ("test_client_state", another_store.state());
  EXPECT_EQ(69, store.file_size_);

  EXPECT_TRUE(another_store.VerifyChecksum());
}

TEST_F(V4StoreTest, WriteToDiskFails) {
  // Pass the directory name as file name so that when the code tries to rename
  // the temp store file to |store_path_| it fails.
  EXPECT_EQ(UNABLE_TO_RENAME_FAILURE,
            InMemoryV4Store(task_runner_, temp_dir_.GetPath())
                .WriteToDisk(Checksum()));

  // Give a location that isn't writable, even for the tmp file.
  base::FilePath non_writable_dir =
      temp_dir_.GetPath()
          .Append(FILE_PATH_LITERAL("nonexistent_dir"))
          .Append(FILE_PATH_LITERAL("some.store"));
  EXPECT_EQ(
      UNEXPECTED_BYTES_WRITTEN_FAILURE,
      InMemoryV4Store(task_runner_, non_writable_dir).WriteToDisk(Checksum()));
}

TEST_F(V4StoreTest, FullUpdateFailsChecksumSynchronously) {
  InMemoryV4Store store(task_runner_, store_path_);
  bool called_back = false;
  UpdatedStoreReadyCallback store_ready_callback =
      base::BindOnce(&V4StoreTest::UpdatedStoreReady, base::Unretained(this),
                     &called_back, false /* expect_store */);
  EXPECT_FALSE(base::PathExists(store.store_path_));
  EXPECT_FALSE(store.HasValidData());  // Never actually read from disk.

  // Now create a response with invalid checksum.
  std::unique_ptr<ListUpdateResponse> lur(new ListUpdateResponse);
  lur->set_response_type(ListUpdateResponse::FULL_UPDATE);
  lur->mutable_checksum()->set_sha256(std::string(crypto::kSHA256Length, 0));
  store.ApplyUpdate(std::move(lur), task_runner_,
                    std::move(store_ready_callback));
  // The update should fail synchronously and not create a store file.
  EXPECT_FALSE(base::PathExists(store.store_path_));

  // Run everything on the task runner to ensure there are no pending tasks.
  task_runner_->RunPendingTasks();
  base::RunLoop().RunUntilIdle();

  // This ensures that the callback was called.
  EXPECT_TRUE(called_back);
  // Ensure that the file is still not created.
  EXPECT_FALSE(base::PathExists(store.store_path_));
  EXPECT_FALSE(updated_store_);
}

TEST_F(V4StoreTest, VerifyChecksumMmapFile) {
  ListUpdateResponse list_update_response;
  list_update_response.set_new_client_state("test_client_state");
  list_update_response.set_platform_type(LINUX_PLATFORM);
  list_update_response.set_response_type(ListUpdateResponse::FULL_UPDATE);

  std::string expected_checksum;
  base::Base64Decode("NrvlDtloQdEEQ7y2cNZVTwo0t2G+Z+ycSorSwMRMpCw=",
                     &expected_checksum);
  list_update_response.mutable_checksum()->set_sha256(expected_checksum);

  base::WriteFile(MmapHashPrefixMap::GetPath(store_path_, "foo"), "abcde");

  V4StoreFileFormat file_format;
  auto* hash_file = file_format.add_hash_files();
  hash_file->set_prefix_size(5);
  hash_file->set_extension("foo");
  hash_file->set_file_size(5);

  WriteFileFormatProtoToFile(&file_format, 0x600D71FE, 9,
                             &list_update_response);
  V4Store store(task_runner_, store_path_,
                std::make_unique<MmapHashPrefixMap>(store_path_));
  EXPECT_TRUE(store.expected_checksum_.empty());

  EXPECT_EQ(READ_SUCCESS, store.ReadFromDisk());
  EXPECT_FALSE(store.expected_checksum_.empty());
  EXPECT_EQ("test_client_state", store.state());
  EXPECT_EQ(85, store.file_size_);

  EXPECT_TRUE(store.VerifyChecksum());

  EXPECT_EQ(store.hash_prefix_map_->view()[5], "abcde");
}

TEST_F(V4StoreTest, FailedMmapOnRead) {
  ListUpdateResponse list_update_response;
  list_update_response.set_new_client_state("test_client_state");
  list_update_response.set_platform_type(LINUX_PLATFORM);
  list_update_response.set_response_type(ListUpdateResponse::FULL_UPDATE);

  V4StoreFileFormat file_format;
  auto* hash_file = file_format.add_hash_files();
  hash_file->set_prefix_size(5);
  hash_file->set_extension("foo");

  WriteFileFormatProtoToFile(&file_format, 0x600D71FE, 9,
                             &list_update_response);
  V4Store store(task_runner_, store_path_,
                std::make_unique<MmapHashPrefixMap>(store_path_));

  EXPECT_EQ(HASH_PREFIX_MAP_GENERATION_FAILURE, store.ReadFromDisk());
}

TEST_F(V4StoreTest, MigrateToMmap) {
  const std::string kFullHash = "abcdefghijklmnopqrstu";
  const std::string kHash = "abcde";
  InMemoryV4Store write_store(task_runner_, store_path_);
  write_store.state_ = "test_client_state";
  write_store.hash_prefix_map_->Append(5, kHash);
  EXPECT_EQ(WRITE_SUCCESS, write_store.WriteToDisk(Checksum()));

  // Make sure an in-memory store can read correctly.
  InMemoryV4Store in_memory_store(task_runner_, store_path_);
  EXPECT_EQ(READ_SUCCESS, in_memory_store.ReadFromDisk());
  EXPECT_EQ("test_client_state", in_memory_store.state());
  EXPECT_EQ(in_memory_store.hash_prefix_map_->view()[5], kHash);
  EXPECT_EQ(in_memory_store.GetMatchingHashPrefix(kFullHash), kHash);

  // Migrate to a mmap store.
  V4Store mmap_store(task_runner_, store_path_,
                     std::make_unique<MmapHashPrefixMap>(store_path_));
  EXPECT_EQ(READ_SUCCESS, mmap_store.ReadFromDisk());
  EXPECT_EQ("test_client_state", mmap_store.state());
  EXPECT_EQ(mmap_store.hash_prefix_map_->view()[5], kHash);
  EXPECT_EQ(mmap_store.GetMatchingHashPrefix(kFullHash), kHash);

  std::string proto_contents;
  EXPECT_TRUE(base::ReadFileToString(store_path_, &proto_contents));
  V4StoreFileFormat file_format;
  EXPECT_TRUE(file_format.ParseFromString(proto_contents));

  EXPECT_EQ(file_format.hash_files().size(), 1);
  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(
      MmapHashPrefixMap::GetPath(store_path_,
                                 file_format.hash_files(0).extension()),
      &contents));
  EXPECT_EQ(contents, kHash);
  EXPECT_EQ(mmap_store.file_size(),
            static_cast<int64_t>(proto_contents.size() + kHash.size()));

  // Reading again should not migrate.
  base::Time last_modified = GetLastModifiedTime(store_path_);
  V4Store mmap_store2(task_runner_, store_path_,
                      std::make_unique<MmapHashPrefixMap>(store_path_));
  EXPECT_EQ(READ_SUCCESS, mmap_store2.ReadFromDisk());
  EXPECT_EQ(GetLastModifiedTime(store_path_), last_modified);
  EXPECT_EQ(mmap_store2.GetMatchingHashPrefix(kFullHash), kHash);
}

TEST_F(V4StoreTest, MigrateToInMemory) {
  const std::string kFullHash = "abcdefghijklmnopqrstu";
  const std::string kHash = "abcde";
  auto mmap_map = std::make_unique<MmapHashPrefixMap>(store_path_);
  auto* mmap_map_raw = mmap_map.get();
  V4Store write_store(task_runner_, store_path_, std::move(mmap_map));
  write_store.state_ = "test_client_state";
  write_store.hash_prefix_map_->Append(5, kHash);
  base::FilePath hashes_path = MmapHashPrefixMap::GetPath(
      store_path_, mmap_map_raw->GetExtensionForTesting(5));
  EXPECT_EQ(WRITE_SUCCESS, write_store.WriteToDisk(Checksum()));
  EXPECT_EQ(write_store.GetMatchingHashPrefix(kFullHash), kHash);

  // Make sure a mmap store can read correctly.
  auto mmap_map2 = std::make_unique<MmapHashPrefixMap>(store_path_);
  auto* mmap_map_raw2 = mmap_map2.get();
  V4Store mmap_store(task_runner_, store_path_, std::move(mmap_map2));
  EXPECT_EQ(READ_SUCCESS, mmap_store.ReadFromDisk());
  EXPECT_EQ("test_client_state", mmap_store.state());
  EXPECT_EQ(mmap_store.hash_prefix_map_->view()[5], kHash);
  EXPECT_TRUE(base::PathExists(hashes_path));
  EXPECT_EQ(mmap_store.GetMatchingHashPrefix(kFullHash), kHash);

  mmap_map_raw->ClearAndWaitForTesting();
  mmap_map_raw2->ClearAndWaitForTesting();

  write_store.Reset();
  mmap_store.Reset();

  // Migrate to an in-memory store.
  InMemoryV4Store in_memory_store(task_runner_, store_path_);
  EXPECT_EQ(READ_SUCCESS, in_memory_store.ReadFromDisk());
  EXPECT_EQ("test_client_state", in_memory_store.state());
  EXPECT_EQ(in_memory_store.hash_prefix_map_->view()[5], kHash);
  EXPECT_EQ(in_memory_store.GetMatchingHashPrefix(kFullHash), kHash);

  EXPECT_FALSE(base::PathExists(hashes_path));

  int64_t file_size;
  EXPECT_TRUE(base::GetFileSize(store_path_, &file_size));
  EXPECT_EQ(in_memory_store.file_size(), file_size);

  // Reading again should not migrate.
  base::Time last_modified = GetLastModifiedTime(store_path_);
  InMemoryV4Store in_memory_store2(task_runner_, store_path_);
  EXPECT_EQ(READ_SUCCESS, in_memory_store2.ReadFromDisk());
  EXPECT_EQ(GetLastModifiedTime(store_path_), last_modified);
  EXPECT_EQ(in_memory_store2.GetMatchingHashPrefix(kFullHash), kHash);
}

TEST_F(V4StoreTest, MigrateFileOffsets) {
  const std::string kFullHash = "abcdefghijklmnopqrstu";
  const std::string kHash = "abcd";
  const std::string kFullHash2 = "zzzzefghijklmnopqrstu";
  const std::string kHash2 = "zzzz";
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kMmapSafeBrowsingDatabase, {{"store-bytes-per-offset", "8"}});
  V4Store write_store(task_runner_, store_path_,
                      std::make_unique<MmapHashPrefixMap>(store_path_));
  write_store.state_ = "test_client_state";
  write_store.hash_prefix_map_->Append(4, kHash + kHash2);
  EXPECT_EQ(WRITE_SUCCESS, write_store.WriteToDisk(Checksum()));
  EXPECT_EQ(write_store.GetMatchingHashPrefix(kFullHash), kHash);
  EXPECT_EQ(write_store.GetMatchingHashPrefix(kFullHash2), kHash2);

  feature_list.Reset();
  feature_list.InitAndEnableFeatureWithParameters(
      kMmapSafeBrowsingDatabase, {{"store-bytes-per-offset", "4"}});

  V4Store mmap_store(task_runner_, store_path_,
                     std::make_unique<MmapHashPrefixMap>(store_path_));

  EXPECT_EQ(READ_SUCCESS, mmap_store.ReadFromDisk());
  EXPECT_EQ("test_client_state", mmap_store.state());
  EXPECT_EQ(mmap_store.GetMatchingHashPrefix(kFullHash), kHash);
  EXPECT_EQ(mmap_store.GetMatchingHashPrefix(kFullHash2), kHash2);
}

TEST_F(V4StoreTest, MigrateToInMemoryFails) {
  ListUpdateResponse list_update_response;
  list_update_response.set_platform_type(LINUX_PLATFORM);
  list_update_response.set_response_type(ListUpdateResponse::FULL_UPDATE);

  V4StoreFileFormat file_format;
  auto* hash_file = file_format.add_hash_files();
  hash_file->set_prefix_size(5);
  // File has not been created.
  hash_file->set_extension("foo");

  WriteFileFormatProtoToFile(&file_format, 0x600D71FE, 9,
                             &list_update_response);

  // Migrate to an in-memory store.
  InMemoryV4Store in_memory_store(task_runner_, store_path_);
  EXPECT_EQ(MIGRATION_FAILURE, in_memory_store.ReadFromDisk());
}

TEST_F(V4StoreTest, CleanUpOldFiles) {
  base::FilePath old_hashes_path =
      MmapHashPrefixMap::GetPath(store_path_, "foo");
  base::WriteFile(old_hashes_path, "abcde");

  base::FilePath other_path = temp_dir_.GetPath().AppendASCII("SomePath");
  base::WriteFile(other_path, "stuff");

  InMemoryV4Store store(task_runner_, store_path_);
  EXPECT_EQ(WRITE_SUCCESS, store.WriteToDisk(Checksum()));

  EXPECT_FALSE(base::PathExists(old_hashes_path));
  EXPECT_TRUE(base::PathExists(other_path));
}

TEST_F(V4StoreTest, FileSizeIncludesHashFiles) {
  V4Store write_store(task_runner_, store_path_,
                      std::make_unique<MmapHashPrefixMap>(store_path_));
  write_store.hash_prefix_map_->Append(4, "abcd");
  EXPECT_EQ(WRITE_SUCCESS, write_store.WriteToDisk(Checksum()));

  int64_t original_file_size = write_store.file_size();

  static_cast<MmapHashPrefixMap*>(write_store.hash_prefix_map_.get())
      ->ClearAndWaitForTesting();
  write_store.Reset();
  write_store.hash_prefix_map_->Append(4, "abcd");
  write_store.hash_prefix_map_->Append(4, "efgh");
  EXPECT_EQ(WRITE_SUCCESS, write_store.WriteToDisk(Checksum()));
  EXPECT_EQ(write_store.file_size(), original_file_size + 4);

  V4Store read_store(task_runner_, store_path_,
                     std::make_unique<MmapHashPrefixMap>(store_path_));
  EXPECT_EQ(READ_SUCCESS, read_store.ReadFromDisk());
  EXPECT_EQ(read_store.file_size(), original_file_size + 4);
}

TEST_F(V4StoreTest, ReserveSpaceInPrefixMap) {
  class ReserveTrackingHashPrefixMap : public InMemoryHashPrefixMap {
   public:
    void Reserve(PrefixSize size, size_t capacity) override {
      reserve_map_[size] = capacity;
    }

    std::unordered_map<PrefixSize, size_t> reserve_map_;
  };

  InMemoryHashPrefixMap old_map;
  InMemoryHashPrefixMap additions_map;
  old_map.Append(4, "abcdefgh");
  old_map.Append(5, "abcdefghij");
  additions_map.Append(4, "123456789012zzzz");
  additions_map.Append(5, "1234567890");

  ReserveTrackingHashPrefixMap reserve_map;
  V4Store::ReserveSpaceInPrefixMap(old_map, additions_map, 0, &reserve_map);

  EXPECT_EQ(reserve_map.reserve_map_[4], 24u);
  EXPECT_EQ(reserve_map.reserve_map_[5], 20u);

  ReserveTrackingHashPrefixMap reserve_map_with_removals;
  V4Store::ReserveSpaceInPrefixMap(old_map, additions_map, 2,
                                   &reserve_map_with_removals);

  EXPECT_EQ(reserve_map_with_removals.reserve_map_[4], 16u);
  EXPECT_EQ(reserve_map_with_removals.reserve_map_[5], 10u);
}

TEST_F(V4StoreTest, MergeUpdatesWithMmapHashPrefixMap) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kMmapSafeBrowsingDatabase, {{"store-bytes-per-offset", "2"}});

  InMemoryHashPrefixMap prefix_map_old;
  prefix_map_old.Append(4, "abcdefgh");
  prefix_map_old.Append(5, "54321abcde");

  InMemoryHashPrefixMap prefix_map_additions;
  prefix_map_additions.Append(4, "----1111bbbb");
  prefix_map_additions.Append(5, "22222bcdef");

  V4Store store(task_runner_, store_path_,
                std::make_unique<MmapHashPrefixMap>(store_path_));
  // Proof of checksum validity using python:
  // >>> import hashlib
  // >>> m = hashlib.sha256()
  // >>> m.update("----11112222254321abcdabcdebbbbbcdefefgh")
  // >>> m.digest()
  // "\xbc\xb3\xedk\xe3x\xd1(\xa9\xedz7]"
  // "x\x18\xbdn]\xa5\xa8R\xf7\xab\xcf\xc1\xa3\xa3\xc5Z,\xa6o"
  std::string expected_checksum(
      "\xBC\xB3\xEDk\xE3x\xD1(\xA9\xEDz7]x\x18\xBDn]"
      "\xA5\xA8R\xF7\xAB\xCF\xC1\xA3\xA3\xC5Z,\xA6o",
      crypto::kSHA256Length);
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            store.MergeUpdate(prefix_map_old, prefix_map_additions, nullptr,
                              expected_checksum));

  EXPECT_EQ(WRITE_SUCCESS, store.WriteToDisk(Checksum()));
  EXPECT_EQ(store.hash_prefix_map_->IsValid(), APPLY_UPDATE_SUCCESS);

  HashPrefixMapView prefix_map = store.hash_prefix_map_->view();
  EXPECT_EQ(2u, prefix_map.size());
  EXPECT_EQ(prefix_map[4], "----1111abcdbbbbefgh");
  EXPECT_EQ(prefix_map[5], "2222254321abcdebcdef");

  std::string proto_contents;
  EXPECT_TRUE(base::ReadFileToString(store_path_, &proto_contents));
  V4StoreFileFormat file_format;
  EXPECT_TRUE(file_format.ParseFromString(proto_contents));

  EXPECT_EQ(file_format.hash_files().size(), 2);
  for (const auto& hash_file : file_format.hash_files())
    EXPECT_EQ(hash_file.offsets().size(), 10);
}

}  // namespace safe_browsing
