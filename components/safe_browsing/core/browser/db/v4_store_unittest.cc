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
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "components/crx_file/id_util.h"
#include "components/safe_browsing/core/browser/db/safebrowsing.pb.h"
#include "components/safe_browsing/core/browser/db/v4_store.pb.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/v5_store.pb.h"
#include "crypto/hash.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"

namespace safe_browsing {

using ::google::protobuf::RepeatedField;
using ::google::protobuf::RepeatedPtrField;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

class V4StoreTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    store_path_ = temp_dir_.GetPath().AppendASCII("V4StoreTest.store");
    v5_store_path_ = temp_dir_.GetPath().AppendASCII("V4StoreTest_v5.store");
    DVLOG(1) << "store_path_: " << store_path_.value();
    DVLOG(1) << "v5_store_path_: " << v5_store_path_.value();
  }

  void TearDown() override {
    base::DeleteFile(store_path_);
    base::DeleteFile(v5_store_path_);
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

  void WriteV5FileFormatProtoToFile(uint32_t magic,
                                    uint32_t file_version = 0,
                                    ListDetails* details = nullptr) {
    V5StoreFileFormat file_format;
    WriteV5FileFormatProtoToFile(&file_format, magic, file_version, details);
  }

  void WriteV5FileFormatProtoToFile(V5StoreFileFormat* file_format,
                                    uint32_t magic,
                                    uint32_t file_version,
                                    ListDetails* details) {
    file_format->set_magic_number(magic);
    file_format->set_file_version(file_version);
    if (details != nullptr) {
      ListDetails* list_details = file_format->mutable_list_details();
      *list_details = *details;
    }

    std::string file_format_string;
    file_format->SerializeToString(&file_format_string);
    base::WriteFile(v5_store_path_, file_format_string);
  }

  void UpdatedStoreReady(base::RunLoop* run_loop,
                         bool expect_store,
                         SBStorePtr store) {
    if (expect_store) {
      ASSERT_TRUE(store);
      V4Store* v4_store = static_cast<V4Store*>(store.get());
      EXPECT_EQ(2u, v4_store->hash_prefix_map_->view().size());
      EXPECT_EQ("22222", v4_store->hash_prefix_map_->view()[5]);
      EXPECT_EQ("abcd", v4_store->hash_prefix_map_->view()[4]);
    } else {
      ASSERT_FALSE(store);
    }

    updated_store_ = std::move(store);
    run_loop->Quit();
  }

  base::Time GetLastModifiedTime(const base::FilePath& path) {
    base::File::Info info;
    base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    EXPECT_TRUE(file.GetInfo(&info));
    return info.last_modified;
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner() {
    return base::SequencedTaskRunner::GetCurrentDefault();
  }

  HashPrefixMapView PrefixMapToView(
      const std::unordered_map<PrefixSize, HashPrefixes>& map) {
    return HashPrefixMapView(map.begin(), map.end());
  }

  std::string ExtensionIdToHash(std::string_view extension_id) {
    return SBStore::ExtensionIdToHash(extension_id);
  }

  StoreReadResult ReadFromDisk(V4Store& store) { return store.ReadFromDisk(); }
  const HashPrefixMap& GetHashPrefixMap(const V4Store& store) {
    return *store.hash_prefix_map_;
  }
  std::string GetExpectedChecksum(const V4Store& store) {
    return store.expected_checksum_;
  }

  void RunExtensionMigrationFailureTest(
      uint64_t v5_hash_file_size,
      std::optional<std::string> v5_hash_file_content,
      base::OnceClosure setup_failure_condition,
      ConvertExtensionBlocklistV5ToV4Result expected_result,
      bool expect_v5_hash_file_deleted,
      base::OnceClosure teardown_cleanup = base::OnceClosure()) {
    base::HistogramTester histograms;
    ListDetails list_details;
    list_details.set_version("v5_version");
    V5HashFile* hash_file = list_details.mutable_hash_file();
    hash_file->set_extension("foo");
    hash_file->set_file_size(v5_hash_file_size);

    WriteV5FileFormatProtoToFile(/*magic=*/0x600D71FE, /*file_version=*/10,
                                 &list_details);

    if (v5_hash_file_content.has_value()) {
      base::WriteFile(v5_store_path_.AddExtensionASCII("foo"),
                      v5_hash_file_content.value());
    }

    if (!setup_failure_condition.is_null()) {
      std::move(setup_failure_condition).Run();
    }

    V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/16,
                  /*is_eligible_for_migration=*/true,
                  /*is_extensions_blocklist=*/true);
    EXPECT_EQ(V5_TO_V4_MIGRATION_FAILURE, ReadFromDisk(store));

    histograms.ExpectUniqueSample(
        "SafeBrowsing.V4Store.ConvertExtensionBlocklistV5ToV4Result",
        expected_result, 1);
    histograms.ExpectUniqueSample(
        "SafeBrowsing.V4Store.V5ToV4MigrationResult",
        V5ToV4MigrationResult::kExtensionBlocklistMigrationFailed, 1);

    EXPECT_FALSE(base::PathExists(v5_store_path_));
    EXPECT_FALSE(base::PathExists(store_path_));
    if (expect_v5_hash_file_deleted) {
      EXPECT_FALSE(base::PathExists(v5_store_path_.AddExtensionASCII("foo")));
    }

    if (!teardown_cleanup.is_null()) {
      std::move(teardown_cleanup).Run();
    }
  }

  void RunExtensionMigrationChecksumTest(
      std::optional<std::string> override_checksum,
      bool expect_success) {
    base::HistogramTester histograms;
    V5StoreFileFormat file_format;
    file_format.set_magic_number(0x600D71FE);
    file_format.set_file_version(10);
    ListDetails* list_details = file_format.mutable_list_details();
    list_details->set_version("v5_version");

    std::string v5_hash_data;
    v5_hash_data.append(ExtensionIdToHash("aapbdbdomjkkjkaonfhkkikfgjllcleb"));
    v5_hash_data.append(ExtensionIdToHash("aapbdbdomjkkjkaonfhkkikfgjllclec"));

    if (override_checksum.has_value()) {
      list_details->mutable_checksum()->set_sha256(override_checksum.value());
    } else {
      // Calculate valid V5 checksum.
      std::array<uint8_t, crypto::hash::kSha256Size> v5_checksum;
      crypto::hash::Hash(crypto::hash::HashKind::kSha256,
                         base::as_byte_span(v5_hash_data), v5_checksum);
      list_details->mutable_checksum()->set_sha256(std::string(
          reinterpret_cast<char*>(v5_checksum.data()), v5_checksum.size()));
    }

    V5HashFile* hash_file = list_details->mutable_hash_file();
    hash_file->set_extension("foo");
    hash_file->set_file_size(v5_hash_data.size());

    // Write V5 store file.
    base::WriteFile(v5_store_path_, file_format.SerializeAsString());
    // Write V5 hash file.
    base::WriteFile(v5_store_path_.AddExtensionASCII("foo"), v5_hash_data);

    V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/16,
                  /*is_eligible_for_migration=*/true,
                  /*is_extensions_blocklist=*/true);
    StoreReadResult expected_read_result =
        expect_success ? READ_SUCCESS : V5_TO_V4_MIGRATION_FAILURE;
    EXPECT_EQ(expected_read_result, ReadFromDisk(store));

    if (expect_success) {
      // Verify V4 files created.
      EXPECT_TRUE(base::PathExists(store_path_));
      EXPECT_TRUE(base::PathExists(store_path_.AddExtensionASCII("32_foo")));

      // Verify V5 files deleted.
      EXPECT_FALSE(base::PathExists(v5_store_path_));
      EXPECT_FALSE(base::PathExists(v5_store_path_.AddExtensionASCII("foo")));
      EXPECT_EQ("v5_version", store.state());

      // Verify data.
      std::string expected_v4_data =
          "aapbdbdomjkkjkaonfhkkikfgjllcleb"
          "aapbdbdomjkkjkaonfhkkikfgjllclec";
      EXPECT_EQ(expected_v4_data, GetHashPrefixMap(store).view().at(32));

      // Verify checksum.
      if (override_checksum != "") {
        std::array<uint8_t, crypto::hash::kSha256Size> expected_checksum;
        crypto::hash::Hash(crypto::hash::HashKind::kSha256,
                           base::as_byte_span(expected_v4_data),
                           expected_checksum);
        EXPECT_EQ(std::string(reinterpret_cast<char*>(expected_checksum.data()),
                              expected_checksum.size()),
                  GetExpectedChecksum(store));
      } else {
        EXPECT_TRUE(GetExpectedChecksum(store).empty());
      }
    } else {
      // Verify files are wiped on failure.
      EXPECT_FALSE(base::PathExists(v5_store_path_));
      EXPECT_FALSE(base::PathExists(v5_store_path_.AddExtensionASCII("foo")));
      EXPECT_FALSE(base::PathExists(store_path_));
    }

    V5ToV4MigrationResult expected_migration_result =
        expect_success
            ? V5ToV4MigrationResult::kV5ToV4MigrationSucceeded
            : V5ToV4MigrationResult::kExtensionBlocklistMigrationFailed;
    ConvertExtensionBlocklistV5ToV4Result expected_conversion_result =
        expect_success
            ? ConvertExtensionBlocklistV5ToV4Result::kSuccess
            : ConvertExtensionBlocklistV5ToV4Result::kV5ChecksumMismatch;

    histograms.ExpectUniqueSample("SafeBrowsing.V4Store.V5ToV4MigrationResult",
                                  expected_migration_result, 1);
    histograms.ExpectUniqueSample(
        "SafeBrowsing.V4Store.ConvertExtensionBlocklistV5ToV4Result",
        expected_conversion_result, 1);
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath store_path_;
  base::FilePath v5_store_path_;
  base::test::TaskEnvironment task_environment_;
  SBStorePtr updated_store_{nullptr, SBStoreDeleter(nullptr)};
};

TEST_F(V4StoreTest, TestReadFromEmptyFile) {
  base::CloseFile(base::OpenFile(store_path_, "wb+"));

  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(FILE_EMPTY_FAILURE, store.ReadFromDisk());
  EXPECT_FALSE(store.HasValidData());
}

TEST_F(V4StoreTest, TestReadFromAbsentFile) {
  EXPECT_EQ(FILE_UNREADABLE_FAILURE,
            V4Store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                    /*is_eligible_for_migration=*/true,
                    /*is_extensions_blocklist=*/false)
                .ReadFromDisk());
}

TEST_F(V4StoreTest, TestReadFromInvalidContentsFile) {
  const char kInvalidContents[] = "Chromium";
  base::WriteFile(store_path_, kInvalidContents);
  EXPECT_EQ(PROTO_PARSING_FAILURE,
            V4Store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                    /*is_eligible_for_migration=*/true,
                    /*is_extensions_blocklist=*/false)
                .ReadFromDisk());
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
            V4Store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                    /*is_eligible_for_migration=*/true,
                    /*is_extensions_blocklist=*/false)
                .ReadFromDisk());
}

TEST_F(V4StoreTest, TestReadFromUnexpectedMagicFile) {
  WriteFileFormatProtoToFile(111);
  EXPECT_EQ(UNEXPECTED_MAGIC_NUMBER_FAILURE,
            V4Store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                    /*is_eligible_for_migration=*/true,
                    /*is_extensions_blocklist=*/false)
                .ReadFromDisk());
}

TEST_F(V4StoreTest, TestReadFromLowVersionFile) {
  WriteFileFormatProtoToFile(0x600D71FE, 2);
  EXPECT_EQ(FILE_VERSION_INCOMPATIBLE_FAILURE,
            V4Store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                    /*is_eligible_for_migration=*/true,
                    /*is_extensions_blocklist=*/false)
                .ReadFromDisk());
}

TEST_F(V4StoreTest, TestReadFromNoHashPrefixInfoFile) {
  WriteFileFormatProtoToFile(0x600D71FE, 9);
  EXPECT_EQ(HASH_PREFIX_INFO_MISSING_FAILURE,
            V4Store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                    /*is_eligible_for_migration=*/true,
                    /*is_extensions_blocklist=*/false)
                .ReadFromDisk());
}

TEST_F(V4StoreTest, TestReadFromNoHashPrefixesFile) {
  ListUpdateResponse list_update_response;
  list_update_response.set_platform_type(LINUX_PLATFORM);
  list_update_response.set_response_type(ListUpdateResponse::FULL_UPDATE);
  WriteFileFormatProtoToFile(0x600D71FE, 9, &list_update_response);
  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(READ_SUCCESS, store.ReadFromDisk());
  EXPECT_TRUE(store.hash_prefix_map_->view().empty());
  EXPECT_EQ(14, store.file_size_);
  EXPECT_FALSE(store.HasValidData());
}

TEST_F(V4StoreTest, TestMigrationAlreadyV4) {
  base::HistogramTester histograms;
  ListUpdateResponse list_update_response;
  list_update_response.set_platform_type(LINUX_PLATFORM);
  list_update_response.set_response_type(ListUpdateResponse::FULL_UPDATE);
  WriteFileFormatProtoToFile(/*magic=*/0x600D71FE, /*version=*/9,
                             &list_update_response);

  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(READ_SUCCESS, store.ReadFromDisk());
  histograms.ExpectUniqueSample("SafeBrowsing.V4Store.V5ToV4MigrationResult",
                                V5ToV4MigrationResult::kDiskAlreadyV4,
                                /*expected_bucket_count=*/1);
}

TEST_F(V4StoreTest, TestMigrationNotEligible_WipeSucceeds) {
  base::HistogramTester histograms;
  V5StoreFileFormat file_format;
  file_format.set_magic_number(0x600D71FE);
  file_format.set_file_version(10);
  ListDetails* list_details = file_format.mutable_list_details();
  list_details->set_version("v5_version");
  V5HashFile* hash_file = list_details->mutable_hash_file();
  hash_file->set_extension("foo");
  hash_file->set_file_size(4);

  // Write V5 store file.
  base::WriteFile(v5_store_path_, file_format.SerializeAsString());
  // Write V5 hash file.
  base::WriteFile(v5_store_path_.AddExtensionASCII("foo"), "abcd");

  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/false,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5_TO_V4_MIGRATION_WIPED_SUCCESSFULLY, store.ReadFromDisk());

  // Verify V5 files are wiped.
  EXPECT_FALSE(base::PathExists(v5_store_path_));
  EXPECT_FALSE(base::PathExists(v5_store_path_.AddExtensionASCII("foo")));

  histograms.ExpectUniqueSample(
      "SafeBrowsing.V4Store.V5ToV4MigrationResult",
      V5ToV4MigrationResult::kStoreIneligibleWipeSucceeded,
      /*expected_bucket_count=*/1);
  histograms.ExpectTotalCount(
      "SafeBrowsing.V4Store.V5ToV4Migration.TimeTaken.V4StoreTest", 1);
}

TEST_F(V4StoreTest, TestMigrationNotEligible_WipeFails) {
  base::HistogramTester histograms;
  V5StoreFileFormat file_format;
  file_format.set_magic_number(0x600D71FE);
  file_format.set_file_version(10);
  ListDetails* list_details = file_format.mutable_list_details();
  list_details->set_version("v5_version");
  V5HashFile* hash_file = list_details->mutable_hash_file();
  hash_file->set_extension("foo");
  hash_file->set_file_size(4);

  base::WriteFile(v5_store_path_, file_format.SerializeAsString());
  base::WriteFile(v5_store_path_.AddExtensionASCII("foo"), "abcd");

  // Force wipe to fail by making the V5 store path a non-empty directory.
  base::DeleteFile(v5_store_path_);
  ASSERT_TRUE(base::CreateDirectory(v5_store_path_));
  ASSERT_TRUE(base::WriteFile(v5_store_path_.AppendASCII("dummy"), "dummy"));

  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/false,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5_TO_V4_MIGRATION_FAILURE, store.ReadFromDisk());

  histograms.ExpectUniqueSample(
      "SafeBrowsing.V4Store.V5ToV4MigrationResult",
      V5ToV4MigrationResult::kStoreIneligibleWipeFailed,
      /*expected_bucket_count=*/1);

  // Cleanup dummy directory.
  base::DeletePathRecursively(v5_store_path_);
}

TEST_F(V4StoreTest,
       TestMigrationNotEligible_WipeHashFileFails_WipeStoreFileSucceeds) {
  base::HistogramTester histograms;
  V5StoreFileFormat file_format;
  file_format.set_magic_number(0x600D71FE);
  file_format.set_file_version(10);
  ListDetails* list_details = file_format.mutable_list_details();
  list_details->set_version("v5_version");
  V5HashFile* hash_file = list_details->mutable_hash_file();
  hash_file->set_extension("foo");
  hash_file->set_file_size(4);

  base::WriteFile(v5_store_path_, file_format.SerializeAsString());

  // Force hash file wipe to fail by making it a non-empty directory.
  base::FilePath hash_file_path = v5_store_path_.AddExtensionASCII("foo");
  ASSERT_TRUE(base::CreateDirectory(hash_file_path));
  ASSERT_TRUE(base::WriteFile(hash_file_path.AppendASCII("dummy"), "dummy"));

  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/false,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5_TO_V4_MIGRATION_FAILURE, store.ReadFromDisk());

  // The V5 hash file still exists because the wipe failed to delete it.
  EXPECT_TRUE(base::PathExists(hash_file_path));
  // But in spite of that, the V5 store file was still able to be deleted.
  EXPECT_FALSE(base::PathExists(v5_store_path_));

  histograms.ExpectUniqueSample(
      "SafeBrowsing.V4Store.V5ToV4MigrationResult",
      V5ToV4MigrationResult::kStoreIneligibleWipeFailed,
      /*expected_bucket_count=*/1);

  // Cleanup dummy directory.
  base::DeletePathRecursively(hash_file_path);
}

TEST_F(V4StoreTest, TestMigrationV5NotFound) {
  base::HistogramTester histograms;
  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(FILE_UNREADABLE_FAILURE, store.ReadFromDisk());
  histograms.ExpectUniqueSample("SafeBrowsing.V4Store.V5ToV4MigrationResult",
                                V5ToV4MigrationResult::kV5StoreNotFound,
                                /*expected_bucket_count=*/1);
}

TEST_F(V4StoreTest, TestMigrationSuccess) {
  V5StoreFileFormat file_format;
  file_format.set_magic_number(0x600D71FE);
  file_format.set_file_version(10);
  ListDetails* list_details = file_format.mutable_list_details();
  list_details->set_version("v5_version");
  list_details->mutable_checksum()->set_sha256("v5_checksum");
  V5HashFile* hash_file = list_details->mutable_hash_file();
  hash_file->set_extension("foo");
  hash_file->set_file_size(4);

  // Write V5 store file.
  base::WriteFile(v5_store_path_, file_format.SerializeAsString());
  // Write V5 hash file.
  base::WriteFile(v5_store_path_.AddExtensionASCII("foo"), "abcd");

  base::HistogramTester histograms;

  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(READ_SUCCESS, store.ReadFromDisk());

  // Verify V4 files created.
  EXPECT_TRUE(base::PathExists(store_path_));
  EXPECT_TRUE(base::PathExists(store_path_.AddExtensionASCII("4_foo")));

  // Verify V5 files deleted.
  EXPECT_FALSE(base::PathExists(v5_store_path_));
  EXPECT_FALSE(base::PathExists(v5_store_path_.AddExtensionASCII("foo")));

  // Verify state.
  EXPECT_EQ("v5_version", store.state());
  EXPECT_EQ("abcd", store.hash_prefix_map_->view()[4]);

  histograms.ExpectUniqueSample(
      "SafeBrowsing.V4Store.V5ToV4MigrationResult",
      V5ToV4MigrationResult::kV5ToV4MigrationSucceeded,
      /*expected_bucket_count=*/1);
  histograms.ExpectTotalCount(
      "SafeBrowsing.V4Store.V5ToV4Migration.TimeTaken.V4StoreTest", 1);
}

TEST_F(V4StoreTest, TestMigrationSuccessNoHashFile) {
  base::HistogramTester histograms;
  ListDetails list_details;
  list_details.set_version("v5_version");
  list_details.mutable_checksum()->set_sha256("v5_checksum");
  // No hash file set.

  WriteV5FileFormatProtoToFile(/*magic=*/0x600D71FE, /*file_version=*/10,
                               &list_details);

  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(READ_SUCCESS, store.ReadFromDisk());

  EXPECT_TRUE(base::PathExists(store_path_));
  EXPECT_FALSE(base::PathExists(v5_store_path_));

  EXPECT_EQ("v5_version", store.state());
  EXPECT_TRUE(store.hash_prefix_map_->view().empty());

  histograms.ExpectUniqueSample(
      "SafeBrowsing.V4Store.V5ToV4MigrationResult",
      V5ToV4MigrationResult::kV5ToV4MigrationSucceeded,
      /*expected_bucket_count=*/1);
}

TEST_F(V4StoreTest, TestMigrationPrefixSizeMismatch) {
  V5StoreFileFormat file_format;
  file_format.set_magic_number(0x600D71FE);
  file_format.set_file_version(10);
  ListDetails* list_details = file_format.mutable_list_details();
  list_details->set_version("v5_version");
  V5HashFile* hash_file = list_details->mutable_hash_file();
  hash_file->set_extension("foo");
  // Set file size to 6, which is not a multiple of 4.
  hash_file->set_file_size(6);

  base::WriteFile(v5_store_path_, file_format.SerializeAsString());
  base::WriteFile(v5_store_path_.AddExtensionASCII("foo"), "abcdef");

  base::HistogramTester histograms;
  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5_TO_V4_MIGRATION_FAILURE, store.ReadFromDisk());
  histograms.ExpectUniqueSample(
      "SafeBrowsing.V4Store.V5ToV4MigrationResult",
      V5ToV4MigrationResult::kPrefixSizeMismatchFailure,
      /*expected_bucket_count=*/1);

  // Verify V5 files are wiped.
  EXPECT_FALSE(base::PathExists(v5_store_path_));
  EXPECT_FALSE(base::PathExists(v5_store_path_.AddExtensionASCII("foo")));
  EXPECT_FALSE(base::PathExists(store_path_));
}

TEST_F(V4StoreTest, TestMigrationHashFileMissing) {
  V5StoreFileFormat file_format;
  file_format.set_magic_number(0x600D71FE);
  file_format.set_file_version(10);
  ListDetails* list_details = file_format.mutable_list_details();
  list_details->set_version("v5_version");
  V5HashFile* hash_file = list_details->mutable_hash_file();
  hash_file->set_extension("foo");
  hash_file->set_file_size(4);

  // Write only V5 store file. Hash file "foo" is missing.
  base::WriteFile(v5_store_path_, file_format.SerializeAsString());

  base::HistogramTester histograms;
  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5_TO_V4_MIGRATION_FAILURE, store.ReadFromDisk());
  histograms.ExpectUniqueSample("SafeBrowsing.V4Store.V5ToV4MigrationResult",
                                V5ToV4MigrationResult::kHashFileMissingFailure,
                                /*expected_bucket_count=*/1);

  EXPECT_FALSE(base::PathExists(v5_store_path_));
}

TEST_F(V4StoreTest, TestMigrationFailureRename) {
  V5StoreFileFormat file_format;
  file_format.set_magic_number(0x600D71FE);
  file_format.set_file_version(10);
  ListDetails* list_details = file_format.mutable_list_details();
  list_details->set_version("v5_version");
  V5HashFile* hash_file = list_details->mutable_hash_file();
  hash_file->set_extension("foo");
  hash_file->set_file_size(4);

  base::WriteFile(v5_store_path_, file_format.SerializeAsString());
  base::WriteFile(v5_store_path_.AddExtensionASCII("foo"), "abcd");

  // Force base::Move to fail by creating a directory at the destination path.
  base::FilePath v4_hash_file_path = store_path_.AddExtensionASCII("4_foo");
  ASSERT_TRUE(base::CreateDirectory(v4_hash_file_path));

  base::HistogramTester histograms;
  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5_TO_V4_MIGRATION_FAILURE, store.ReadFromDisk());
  histograms.ExpectUniqueSample("SafeBrowsing.V4Store.V5ToV4MigrationResult",
                                V5ToV4MigrationResult::kRenameHashFileFailure,
                                /*expected_bucket_count=*/1);

  // Verify V5 files are wiped.
  EXPECT_FALSE(base::PathExists(v5_store_path_));
  EXPECT_FALSE(base::PathExists(v5_store_path_.AddExtensionASCII("foo")));
  EXPECT_FALSE(base::PathExists(v4_hash_file_path));
  EXPECT_FALSE(base::PathExists(store_path_));
}

TEST_F(V4StoreTest, TestMigrationFailureWrite) {
  V5StoreFileFormat file_format;
  file_format.set_magic_number(0x600D71FE);
  file_format.set_file_version(10);
  ListDetails* list_details = file_format.mutable_list_details();
  list_details->set_version("v5_version");
  V5HashFile* hash_file = list_details->mutable_hash_file();
  hash_file->set_extension("foo");
  hash_file->set_file_size(4);

  base::WriteFile(v5_store_path_, file_format.SerializeAsString());
  base::WriteFile(v5_store_path_.AddExtensionASCII("foo"), "abcd");

  // Force base::WriteFile to fail by creating a directory at the temp store
  // path.
  base::FilePath temp_store_path = store_path_.AddExtensionASCII("tmp");
  ASSERT_TRUE(base::CreateDirectory(temp_store_path));

  base::HistogramTester histograms;
  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5_TO_V4_MIGRATION_FAILURE, store.ReadFromDisk());
  histograms.ExpectUniqueSample("SafeBrowsing.V4Store.V5ToV4MigrationResult",
                                V5ToV4MigrationResult::kWriteV4FileFailure,
                                /*expected_bucket_count=*/1);

  // Verify V5 files are wiped.
  EXPECT_FALSE(base::PathExists(v5_store_path_));
  EXPECT_FALSE(base::PathExists(v5_store_path_.AddExtensionASCII("foo")));

  // Cleanup directory.
  base::DeletePathRecursively(temp_store_path);
}

TEST_F(V4StoreTest, TestMigrationFailureRenameV4Store) {
  V5StoreFileFormat file_format;
  file_format.set_magic_number(0x600D71FE);
  file_format.set_file_version(10);
  ListDetails* list_details = file_format.mutable_list_details();
  list_details->set_version("v5_version");
  V5HashFile* hash_file = list_details->mutable_hash_file();
  hash_file->set_extension("foo");
  hash_file->set_file_size(4);

  base::WriteFile(v5_store_path_, file_format.SerializeAsString());
  base::WriteFile(v5_store_path_.AddExtensionASCII("foo"), "abcd");

  // Force base::Move to fail by creating a directory at the destination path.
  ASSERT_TRUE(base::CreateDirectory(store_path_));

  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  // Call MigrateFromV5 directly to bypass PathExists check in
  // AttemptV5ToV4Migration
  EXPECT_EQ(V5ToV4MigrationResult::kRenameV4StoreFileFailure,
            store.MigrateFromV5(v5_store_path_));

  // Verify V5 files are wiped.
  EXPECT_FALSE(base::PathExists(v5_store_path_));
  EXPECT_FALSE(base::PathExists(v5_store_path_.AddExtensionASCII("foo")));

  // Cleanup directory.
  base::DeletePathRecursively(store_path_);
}

TEST_F(V4StoreTest, TestMigrationSuccessButReadFailure) {
  V5StoreFileFormat file_format;
  file_format.set_magic_number(0x600D71FE);
  file_format.set_file_version(10);
  ListDetails* list_details = file_format.mutable_list_details();
  list_details->set_version("v5_version");
  V5HashFile* hash_file = list_details->mutable_hash_file();
  hash_file->set_extension("foo");
  hash_file->set_file_size(4);

  base::WriteFile(v5_store_path_, file_format.SerializeAsString());
  // Write corrupted hash file (only 2 bytes, expected 4).
  base::WriteFile(v5_store_path_.AddExtensionASCII("foo"), "ab");

  base::HistogramTester histograms;
  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);

  // Migration itself succeeds (logs kV5ToV4MigrationSucceeded), but reading it
  // fails because the hash file is corrupted.
  EXPECT_EQ(HASH_PREFIX_MAP_GENERATION_FAILURE, store.ReadFromDisk());

  histograms.ExpectUniqueSample(
      "SafeBrowsing.V4Store.V5ToV4MigrationResult",
      V5ToV4MigrationResult::kV5ToV4MigrationSucceeded,
      /*expected_bucket_count=*/1);
}

TEST_F(V4StoreTest, TestMigrationFailureInvalidV5) {
  // Write V5 file with bad magic.
  WriteV5FileFormatProtoToFile(/*magic=*/111, /*file_version=*/10);

  base::HistogramTester histograms;
  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5_TO_V4_MIGRATION_FAILURE, store.ReadFromDisk());
  histograms.ExpectUniqueSample("SafeBrowsing.V4Store.V5ToV4MigrationResult",
                                V5ToV4MigrationResult::kReadV5Failed,
                                /*expected_bucket_count=*/1);
  histograms.ExpectUniqueSample(
      "SafeBrowsing.V4Store.V5ToV4Migration.V5ReadFailureReason",
      V5StoreReadResult::kUnexpectedMagicNumberFailure,
      /*expected_bucket_count=*/1);

  EXPECT_FALSE(base::PathExists(v5_store_path_));
}

TEST_F(V4StoreTest, TestMigrationInterruptedWipesEverything) {
  V5StoreFileFormat file_format;
  file_format.set_magic_number(0x600D71FE);
  file_format.set_file_version(10);
  ListDetails* list_details = file_format.mutable_list_details();
  list_details->set_version("v5_version");
  list_details->mutable_checksum()->set_sha256("v5_checksum");
  V5HashFile* hash_file = list_details->mutable_hash_file();
  hash_file->set_extension("foo");
  hash_file->set_file_size(4);

  // Write V5 store file.
  base::WriteFile(v5_store_path_, file_format.SerializeAsString());

  // Simulate interrupted migration: The hash file was already moved to V4 path,
  // but V5 store file still exists and V4 store file does not.
  base::FilePath v4_hash_file_path = store_path_.AddExtensionASCII("4_foo");
  base::WriteFile(v4_hash_file_path, "abcd");

  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5_TO_V4_MIGRATION_FAILURE, store.ReadFromDisk());

  // Verify everything is wiped.
  EXPECT_FALSE(base::PathExists(v5_store_path_));
  EXPECT_FALSE(base::PathExists(v4_hash_file_path));
  EXPECT_FALSE(base::PathExists(store_path_));
}

TEST_F(V4StoreTest, TestExtensionMigrationCleanupOnLateFailure) {
  base::HistogramTester histograms;
  V5StoreFileFormat file_format;
  file_format.set_magic_number(0x600D71FE);
  file_format.set_file_version(10);
  ListDetails* list_details = file_format.mutable_list_details();
  list_details->set_version("v5_version");

  std::string v5_hash_data;
  v5_hash_data.append(ExtensionIdToHash("aapbdbdomjkkjkaonfhkkikfgjllcleb"));
  v5_hash_data.append(ExtensionIdToHash("aapbdbdomjkkjkaonfhkkikfgjllclec"));

  std::array<uint8_t, crypto::hash::kSha256Size> v5_checksum;
  crypto::hash::Hash(crypto::hash::HashKind::kSha256,
                     base::as_byte_span(v5_hash_data), v5_checksum);
  list_details->mutable_checksum()->set_sha256(std::string(
      reinterpret_cast<char*>(v5_checksum.data()), v5_checksum.size()));

  V5HashFile* hash_file = list_details->mutable_hash_file();
  hash_file->set_extension("foo");
  hash_file->set_file_size(v5_hash_data.size());

  base::WriteFile(v5_store_path_, file_format.SerializeAsString());
  base::WriteFile(v5_store_path_.AddExtensionASCII("foo"), v5_hash_data);

  // Force failure when writing temp_store_path by creating a directory there.
  base::FilePath temp_store_path = store_path_.AddExtensionASCII("tmp");
  ASSERT_TRUE(base::CreateDirectory(temp_store_path));

  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/16,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/true);
  EXPECT_EQ(V5_TO_V4_MIGRATION_FAILURE, ReadFromDisk(store));

  // Verify V5 files are wiped.
  EXPECT_FALSE(base::PathExists(v5_store_path_));
  EXPECT_FALSE(base::PathExists(v5_store_path_.AddExtensionASCII("foo")));

  // Verify converted hash file is wiped.
  base::FilePath v4_hash_file_path = store_path_.AddExtensionASCII("32_foo");
  EXPECT_FALSE(base::PathExists(v4_hash_file_path));

  // Clean up the directory we created so it doesn't affect other tests.
  base::DeletePathRecursively(temp_store_path);

  histograms.ExpectUniqueSample("SafeBrowsing.V4Store.V5ToV4MigrationResult",
                                V5ToV4MigrationResult::kWriteV4FileFailure, 1);
}

TEST_F(V4StoreTest, TestExtensionMigrationSuccess) {
  base::HistogramTester histograms;
  V5StoreFileFormat file_format;
  file_format.set_magic_number(0x600D71FE);
  file_format.set_file_version(10);
  ListDetails* list_details = file_format.mutable_list_details();
  list_details->set_version("v5_version");

  // We will write V5 hashes.
  std::string v5_hash_data;
  v5_hash_data.append(ExtensionIdToHash("aapbdbdomjkkjkaonfhkkikfgjllcleb"));
  v5_hash_data.append(ExtensionIdToHash("aapbdbdomjkkjkaonfhkkikfgjllclec"));

  V5HashFile* hash_file = list_details->mutable_hash_file();
  hash_file->set_extension("foo");
  hash_file->set_file_size(v5_hash_data.size());

  // Write V5 store file.
  base::WriteFile(v5_store_path_, file_format.SerializeAsString());
  // Write V5 hash file.
  base::WriteFile(v5_store_path_.AddExtensionASCII("foo"), v5_hash_data);

  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/16,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/true);
  EXPECT_EQ(READ_SUCCESS, ReadFromDisk(store));

  // Verify V4 files created.
  EXPECT_TRUE(base::PathExists(store_path_));
  EXPECT_TRUE(base::PathExists(store_path_.AddExtensionASCII("32_foo")));

  // Verify V5 files deleted.
  EXPECT_FALSE(base::PathExists(v5_store_path_));
  EXPECT_FALSE(base::PathExists(v5_store_path_.AddExtensionASCII("foo")));
  EXPECT_EQ("v5_version", store.state());

  // Expected V4 IDs (32 bytes each).
  std::string expected_v4_data =
      "aapbdbdomjkkjkaonfhkkikfgjllcleb"
      "aapbdbdomjkkjkaonfhkkikfgjllclec";
  EXPECT_EQ(expected_v4_data, GetHashPrefixMap(store).view().at(32));

  // Verify checksum is not written because the source had no checksum.
  EXPECT_TRUE(GetExpectedChecksum(store).empty());

  // Verify UMA.
  histograms.ExpectUniqueSample(
      "SafeBrowsing.V4Store.V5ToV4MigrationResult",
      V5ToV4MigrationResult::kV5ToV4MigrationSucceeded, 1);
  histograms.ExpectUniqueSample(
      "SafeBrowsing.V4Store.ConvertExtensionBlocklistV5ToV4Result",
      ConvertExtensionBlocklistV5ToV4Result::kSuccess, 1);
}

TEST_F(V4StoreTest, TestExtensionMigrationFailureInvalidFileSize) {
  RunExtensionMigrationFailureTest(
      /*v5_hash_file_size=*/16,
      /*v5_hash_file_content=*/"123456789012345",
      /*setup_failure_condition=*/base::OnceClosure(),
      /*expected_result=*/
      ConvertExtensionBlocklistV5ToV4Result::kInvalidFileSize,
      /*expect_v5_hash_file_deleted=*/true);
}

TEST_F(V4StoreTest, TestExtensionMigrationFailureReadV5) {
  base::FilePath v5_hash_path = v5_store_path_.AddExtensionASCII("foo");
  RunExtensionMigrationFailureTest(
      /*v5_hash_file_size=*/16,
      /*v5_hash_file_content=*/std::nullopt,
      /*setup_failure_condition=*/
      base::BindOnce(
          [](const base::FilePath& path) {
            ASSERT_TRUE(base::CreateDirectory(path));
          },
          v5_hash_path),
      /*expected_result=*/ConvertExtensionBlocklistV5ToV4Result::kReadV5Failed,
      /*expect_v5_hash_file_deleted=*/false,
      /*teardown_cleanup=*/
      base::BindOnce(
          [](const base::FilePath& path) { base::DeletePathRecursively(path); },
          v5_hash_path));
}

TEST_F(V4StoreTest, TestExtensionMigrationFailureWriteV4) {
  base::FilePath v4_hash_path = store_path_.AddExtensionASCII("32_foo");
  RunExtensionMigrationFailureTest(
      /*v5_hash_file_size=*/16,
      /*v5_hash_file_content=*/"1234567890123456",
      /*setup_failure_condition=*/
      base::BindOnce(
          [](const base::FilePath& path) {
            ASSERT_TRUE(base::CreateDirectory(path));
          },
          v4_hash_path),
      /*expected_result=*/ConvertExtensionBlocklistV5ToV4Result::kWriteV4Failed,
      /*expect_v5_hash_file_deleted=*/true,
      /*teardown_cleanup=*/
      base::BindOnce(
          [](const base::FilePath& path) { base::DeletePathRecursively(path); },
          v4_hash_path));
}

TEST_F(V4StoreTest, TestExtensionMigrationNoHashFiles) {
  base::HistogramTester histograms;
  ListDetails list_details;
  list_details.set_version("v5_version");
  list_details.mutable_checksum()->set_sha256("v5_checksum");

  WriteV5FileFormatProtoToFile(/*magic=*/0x600D71FE, /*file_version=*/10,
                               &list_details);

  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/16,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/true);
  EXPECT_EQ(READ_SUCCESS, ReadFromDisk(store));

  EXPECT_TRUE(base::PathExists(store_path_));
  EXPECT_FALSE(base::PathExists(v5_store_path_));
  EXPECT_EQ("v5_version", store.state());
  EXPECT_TRUE(GetHashPrefixMap(store).view().empty());

  histograms.ExpectTotalCount(
      "SafeBrowsing.V4Store.ConvertExtensionBlocklistV5ToV4Result", 0);
  histograms.ExpectUniqueSample(
      "SafeBrowsing.V4Store.V5ToV4MigrationResult",
      V5ToV4MigrationResult::kV5ToV4MigrationSucceeded, 1);
}

TEST_F(V4StoreTest, TestExtensionMigrationFailureChecksumMismatch) {
  std::string dummy_checksum(32, 'x');
  RunExtensionMigrationChecksumTest(
      /*override_checksum=*/dummy_checksum,
      /*expect_success=*/false);
}

TEST_F(V4StoreTest, TestExtensionMigrationSuccessWithChecksum) {
  RunExtensionMigrationChecksumTest(
      /*override_checksum=*/std::nullopt,
      /*expect_success=*/true);
}

TEST_F(V4StoreTest, TestExtensionMigrationSuccessWithEmptyChecksum) {
  RunExtensionMigrationChecksumTest(
      /*override_checksum=*/"",
      /*expect_success=*/true);
}

TEST_F(V4StoreTest, TestAddUnlumpedHashesWithInvalidAddition) {
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map;
  EXPECT_EQ(ADDITIONS_SIZE_UNEXPECTED_FAILURE,
            V4Store::AddUnlumpedHashes(5, "a", &prefix_map));
  EXPECT_TRUE(prefix_map.empty());
}

TEST_F(V4StoreTest, TestAddUnlumpedHashesWithEmptyString) {
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "", &prefix_map));
  EXPECT_TRUE(prefix_map[5].empty());
}

TEST_F(V4StoreTest, TestAddUnlumpedHashesWithTooSmallPrefixSize) {
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map;
  EXPECT_EQ(PREFIX_SIZE_TOO_SMALL_FAILURE,
            V4Store::AddUnlumpedHashes(3, "abcde5432100000-----", &prefix_map));
  EXPECT_TRUE(prefix_map.empty());
}

TEST_F(V4StoreTest, TestAddUnlumpedHashesWithTooLargePrefixSize) {
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map;
  EXPECT_EQ(
      PREFIX_SIZE_TOO_LARGE_FAILURE,
      V4Store::AddUnlumpedHashes(33, "abcde5432100000-----", &prefix_map));
  EXPECT_TRUE(prefix_map.empty());
}

TEST_F(V4StoreTest, TestAddUnlumpedHashes) {
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map;
  PrefixSize prefix_size = 5;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(prefix_size, "abcde5432100000-----",
                                       &prefix_map));
  EXPECT_EQ(1u, prefix_map.size());
  HashPrefixesView hash_prefixes = prefix_map.at(prefix_size);
  EXPECT_EQ(4 * prefix_size, hash_prefixes.size());
  EXPECT_EQ("abcde5432100000-----", hash_prefixes);

  prefix_size = 4;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(prefix_size, "abcde5432100000-----",
                                       &prefix_map));
  EXPECT_EQ(2u, prefix_map.size());
  hash_prefixes = prefix_map.at(prefix_size);
  EXPECT_EQ(5 * prefix_size, hash_prefixes.size());
  EXPECT_EQ("abcde5432100000-----", hash_prefixes);
}

TEST_F(V4StoreTest, TestGetNextSmallestUnmergedPrefixWithEmptyPrefixMap) {
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map;
  IteratorMap iterator_map;
  V4Store::InitializeIteratorMap(PrefixMapToView(prefix_map), &iterator_map);

  HashPrefixStr prefix;
  EXPECT_FALSE(V4Store::GetNextSmallestUnmergedPrefix(
      PrefixMapToView(prefix_map), iterator_map, &prefix));
}

TEST_F(V4StoreTest, TestGetNextSmallestUnmergedPrefix) {
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "-----0000054321abcde", &prefix_map));
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "*****0000054321abcde", &prefix_map));
  IteratorMap iterator_map;
  V4Store::InitializeIteratorMap(PrefixMapToView(prefix_map), &iterator_map);

  HashPrefixStr prefix;
  EXPECT_TRUE(V4Store::GetNextSmallestUnmergedPrefix(
      PrefixMapToView(prefix_map), iterator_map, &prefix));
  EXPECT_EQ("****", prefix);
}

TEST_F(V4StoreTest, TestMergeUpdatesWithSameSizesInEachMap) {
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "abcdefgh", &prefix_map_old));
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "54321abcde", &prefix_map_old));
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_additions;
  EXPECT_EQ(
      APPLY_UPDATE_SUCCESS,
      V4Store::AddUnlumpedHashes(4, "----1111bbbb", &prefix_map_additions));
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "22222bcdef", &prefix_map_additions));

  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
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
      crypto::hash::kSha256Size);
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            store.MergeUpdate(PrefixMapToView(prefix_map_old),
                              PrefixMapToView(prefix_map_additions), nullptr,
                              expected_checksum));
  V4StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(store.hash_prefix_map_->WriteToDisk(sb_file_format));

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
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "1111abcdefgh", &prefix_map_old));
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "22222bcdef", &prefix_map_additions));

  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  std::string expected_checksum = std::string(
      "\xA5\x8B\xCAsD\xC7\xF9\xCE\xD2\xF4\x4="
      "\xB2\"\x82\x1A\xC1\xB8\x1F\x10\r\v\x9A\x93\xFD\xE1\xB8"
      "B\x1Eh\xF7\xB4",
      crypto::hash::kSha256Size);
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            store.MergeUpdate(PrefixMapToView(prefix_map_old),
                              PrefixMapToView(prefix_map_additions), nullptr,
                              expected_checksum));
  V4StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(store.hash_prefix_map_->WriteToDisk(sb_file_format));

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
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "00001111", &prefix_map_old));
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "2222", &prefix_map_additions));

  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  std::string expected_checksum = std::string(
      "\x84\x92\xET\xED\xF7\x97"
      "C\xCE}\xFF"
      "E\x1\xAB-\b>\xDB\x95\b\xD8H\xD5\x1D\xF9]8x\xA4\xD4\xC2\xFA",
      crypto::hash::kSha256Size);
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            store.MergeUpdate(PrefixMapToView(prefix_map_old),
                              PrefixMapToView(prefix_map_additions), nullptr,
                              expected_checksum));
  V4StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(store.hash_prefix_map_->WriteToDisk(sb_file_format));

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
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "2222", &prefix_map_old));
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "00001111", &prefix_map_additions));

  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  std::string expected_checksum = std::string(
      "\x84\x92\xET\xED\xF7\x97"
      "C\xCE}\xFF"
      "E\x1\xAB-\b>\xDB\x95\b\xD8H\xD5\x1D\xF9]8x\xA4\xD4\xC2\xFA",
      crypto::hash::kSha256Size);
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            store.MergeUpdate(PrefixMapToView(prefix_map_old),
                              PrefixMapToView(prefix_map_additions), nullptr,
                              expected_checksum));
  V4StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(store.hash_prefix_map_->WriteToDisk(sb_file_format));

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
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "2222", &prefix_map_old));
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "2222", &prefix_map_additions));

  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  std::string expected_checksum;
  EXPECT_EQ(ADDITIONS_HAS_EXISTING_PREFIX_FAILURE,
            store.MergeUpdate(PrefixMapToView(prefix_map_old),
                              PrefixMapToView(prefix_map_additions), nullptr,
                              expected_checksum));
}

TEST_F(V4StoreTest, TestMergeUpdatesFailsWhenRemovalsIndexTooLarge) {
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "2222", &prefix_map_old));
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "11113333", &prefix_map_additions));

  // Even though the merged map could have size 3 without removals, the
  // removals index should only count the entries in the old map.
  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  RepeatedField<int32_t> raw_removals;
  // old_store: ["2222"]
  raw_removals.Add(1);
  std::string expected_checksum;
  EXPECT_EQ(REMOVALS_INDEX_TOO_LARGE_FAILURE,
            store.MergeUpdate(PrefixMapToView(prefix_map_old),
                              PrefixMapToView(prefix_map_additions),
                              &raw_removals, expected_checksum));
}

TEST_F(V4StoreTest, TestMergeUpdatesFailsWhenRemovalsIndexNegative) {
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "2222", &prefix_map_old));
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "11113333", &prefix_map_additions));

  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  RepeatedField<int32_t> raw_removals;
  raw_removals.Add(-1);
  std::string expected_checksum;
  EXPECT_EQ(REMOVALS_INDEX_NEGATIVE_FAILURE,
            store.MergeUpdate(PrefixMapToView(prefix_map_old),
                              PrefixMapToView(prefix_map_additions),
                              &raw_removals, expected_checksum));
}

TEST_F(V4StoreTest, TestMergeUpdateFastPathWithRemovals) {
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "0000111122223333444455556666",
                                       &prefix_map_old));
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_additions;
  EXPECT_EQ(
      APPLY_UPDATE_SUCCESS,
      V4Store::AddUnlumpedHashes(4, "1515252550507777", &prefix_map_additions));

  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  RepeatedField<int32_t> raw_removals;
  // Remove "1111" (index 1), "4444" (index 4)
  raw_removals.Add(1);
  raw_removals.Add(4);

  // Remaining: 0000, 2222, 3333, 5555, 6666
  // Additions: 1515, 2525, 5050, 7777
  // Resulting sorted: 0000, 1515, 2222, 2525, 3333, 5050, 5555, 6666, 7777

  crypto::hash::Hasher checksum_ctx(crypto::hash::HashKind::kSha256);
  checksum_ctx.Update("000015152222252533335050555566667777");
  std::array<uint8_t, crypto::hash::kSha256Size> checksum;
  checksum_ctx.Finish(checksum);
  std::string expected_checksum(checksum.begin(), checksum.end());

  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            store.MergeUpdate(PrefixMapToView(prefix_map_old),
                              PrefixMapToView(prefix_map_additions),
                              &raw_removals, expected_checksum));

  V4StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(store.hash_prefix_map_->WriteToDisk(sb_file_format));

  EXPECT_THAT(
      store.hash_prefix_map_->view(),
      UnorderedElementsAre(Pair(4, "000015152222252533335050555566667777")));
}

TEST_F(V4StoreTest, TestMergeUpdateFastPathEmptyLists) {
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_old;
  V4Store::AddUnlumpedHashes(4, "11112222", &prefix_map_old);
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_empty;
  prefix_map_empty[4] = "";

  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            store.MergeUpdate(PrefixMapToView(prefix_map_old),
                              PrefixMapToView(prefix_map_empty), nullptr, ""));
  V4StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(store.hash_prefix_map_->WriteToDisk(sb_file_format));
  EXPECT_THAT(store.hash_prefix_map_->view(),
              UnorderedElementsAre(Pair(4, "11112222")));

  store.hash_prefix_map_->Clear();
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            store.MergeUpdate(PrefixMapToView(prefix_map_empty),
                              PrefixMapToView(prefix_map_old), nullptr, ""));
  EXPECT_TRUE(store.hash_prefix_map_->WriteToDisk(sb_file_format));
  EXPECT_THAT(store.hash_prefix_map_->view(),
              UnorderedElementsAre(Pair(4, "11112222")));
}

TEST_F(V4StoreTest, TestMergeUpdateFastPathMultipleRemovalsInARow) {
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_old;
  V4Store::AddUnlumpedHashes(4, "0000111122223333", &prefix_map_old);
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_additions;
  V4Store::AddUnlumpedHashes(4, "1515", &prefix_map_additions);

  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  RepeatedField<int32_t> raw_removals;
  raw_removals.Add(1);
  raw_removals.Add(2);

  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            store.MergeUpdate(PrefixMapToView(prefix_map_old),
                              PrefixMapToView(prefix_map_additions),
                              &raw_removals, ""));
  V4StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(store.hash_prefix_map_->WriteToDisk(sb_file_format));
  EXPECT_THAT(store.hash_prefix_map_->view(),
              UnorderedElementsAre(Pair(4, "000015153333")));
}

TEST_F(V4StoreTest, TestVerifyChecksumFastPath) {
  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  store.hash_prefix_map_->Append(4, "000011112222");

  crypto::hash::Hasher checksum_ctx(crypto::hash::HashKind::kSha256);
  checksum_ctx.Update("000011112222");
  std::array<uint8_t, crypto::hash::kSha256Size> checksum;
  checksum_ctx.Finish(checksum);
  store.expected_checksum_ = std::string(checksum.begin(), checksum.end());

  V4StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(store.hash_prefix_map_->WriteToDisk(sb_file_format));

  EXPECT_TRUE(store.VerifyChecksum());

  store.expected_checksum_ = std::string(32, '0');
  EXPECT_FALSE(store.VerifyChecksum());
}

TEST_F(V4StoreTest, TestVerifyChecksumValidStoreChecksumEmptyHistogram) {
  base::HistogramTester histograms;
  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  store.hash_prefix_map_->Append(4, "000011112222");
  store.has_valid_data_ = true;

  // Case 1: expected_checksum_ is empty.
  store.expected_checksum_ = "";
  EXPECT_TRUE(store.VerifyChecksum());
  histograms.ExpectUniqueSample(
      "SafeBrowsing.V4Store.VerifyChecksum.ValidStoreChecksumEmpty.V4StoreTest",
      true, 1);
  histograms.ExpectUniqueSample(
      "SafeBrowsing.V4Store.VerifyChecksum.ValidStoreChecksumEmpty", true, 1);

  // Case 2: expected_checksum_ is not empty.
  crypto::hash::Hasher checksum_ctx(crypto::hash::HashKind::kSha256);
  checksum_ctx.Update("000011112222");
  std::array<uint8_t, crypto::hash::kSha256Size> checksum;
  checksum_ctx.Finish(checksum);
  store.expected_checksum_ = std::string(checksum.begin(), checksum.end());

  V4StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(store.hash_prefix_map_->WriteToDisk(sb_file_format));

  EXPECT_TRUE(store.VerifyChecksum());
  // We expect one more sample for "false" (not empty).
  histograms.ExpectBucketCount(
      "SafeBrowsing.V4Store.VerifyChecksum.ValidStoreChecksumEmpty.V4StoreTest",
      false, 1);
  histograms.ExpectTotalCount(
      "SafeBrowsing.V4Store.VerifyChecksum.ValidStoreChecksumEmpty.V4StoreTest",
      2);

  histograms.ExpectBucketCount(
      "SafeBrowsing.V4Store.VerifyChecksum.ValidStoreChecksumEmpty", false, 1);
  histograms.ExpectTotalCount(
      "SafeBrowsing.V4Store.VerifyChecksum.ValidStoreChecksumEmpty", 2);
}

TEST_F(V4StoreTest, TestMergeUpdatesRemovesOnlyElement) {
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "2222", &prefix_map_old));
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "1111133333", &prefix_map_additions));

  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  RepeatedField<int32_t> raw_removals;
  // old_store: ["2222"]
  raw_removals.Add(0);  // Removes "2222"
  std::string expected_checksum = std::string(
      "\xE6\xB0\x1\x12\x89\x83\xF0/"
      "\xE7\xD2\xE6\xDC\x16\xB9\x8C+\xA2\xB3\x9E\x89<,\x88"
      "B3\xA5\xB1"
      "D\x9E\x9E'\x14",
      crypto::hash::kSha256Size);
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            store.MergeUpdate(PrefixMapToView(prefix_map_old),
                              PrefixMapToView(prefix_map_additions),
                              &raw_removals, expected_checksum));
  V4StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(store.hash_prefix_map_->WriteToDisk(sb_file_format));

  EXPECT_THAT(store.hash_prefix_map_->view(),
              UnorderedElementsAre(Pair(5, "1111133333")));
}

TEST_F(V4StoreTest, TestMergeUpdatesRemovesFirstElement) {
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "22224444", &prefix_map_old));
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "1111133333", &prefix_map_additions));

  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  RepeatedField<int32_t> raw_removals;
  // old_store: ["2222", "4444"]
  raw_removals.Add(0);  // Removes "2222"
  std::string expected_checksum = std::string(
      "\x9D\xF3\xF2\x82\0\x1E{\xDF\xCD\xC0V\xBE\xD6<\x85"
      "D7=\xB5v\xAD\b1\xC9\xB3"
      "A\xAC"
      "b\xF1lf\xA4",
      crypto::hash::kSha256Size);
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            store.MergeUpdate(PrefixMapToView(prefix_map_old),
                              PrefixMapToView(prefix_map_additions),
                              &raw_removals, expected_checksum));
  V4StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(store.hash_prefix_map_->WriteToDisk(sb_file_format));

  HashPrefixMapView prefix_map = store.hash_prefix_map_->view();
  // The size is 2 since we reserve space anyway.
  EXPECT_EQ(2u, prefix_map.size());
  EXPECT_EQ("4444", prefix_map.at(4));
  EXPECT_EQ("1111133333", prefix_map.at(5));
}

TEST_F(V4StoreTest, TestMergeUpdatesRemovesMiddleElement) {
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "222233334444", &prefix_map_old));
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "1111133333", &prefix_map_additions));

  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  RepeatedField<int32_t> raw_removals;
  // old_store: ["2222", "3333", 4444"]
  raw_removals.Add(1);  // Removes "3333"
  std::string expected_checksum = std::string(
      "\xFA-A\x15{\x17\0>\xAE"
      "8\xACigR\xD1\x93<\xB2\xC9\xB5\x81\xC0\xFB\xBB\x2\f\xAFpN\xEA"
      "44",
      crypto::hash::kSha256Size);
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            store.MergeUpdate(PrefixMapToView(prefix_map_old),
                              PrefixMapToView(prefix_map_additions),
                              &raw_removals, expected_checksum));
  V4StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(store.hash_prefix_map_->WriteToDisk(sb_file_format));

  HashPrefixMapView prefix_map = store.hash_prefix_map_->view();
  // The size is 2 since we reserve space anyway.
  EXPECT_EQ(2u, prefix_map.size());
  EXPECT_EQ("22224444", prefix_map.at(4));
  EXPECT_EQ("1111133333", prefix_map.at(5));
}

TEST_F(V4StoreTest, TestMergeUpdatesRemovesLastElement) {
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "222233334444", &prefix_map_old));
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "1111133333", &prefix_map_additions));

  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  RepeatedField<int32_t> raw_removals;
  // old_store: ["2222", "3333", 4444"]
  raw_removals.Add(2);  // Removes "4444"
  std::string expected_checksum = std::string(
      "a\xE1\xAD\x96\xFE\xA6"
      "A\xCA~7W\xF6z\xD8\n\xCA?\x96\x8A\x17U\x5\v\r\x88]\n\xB2JX\xC4S",
      crypto::hash::kSha256Size);
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            store.MergeUpdate(PrefixMapToView(prefix_map_old),
                              PrefixMapToView(prefix_map_additions),
                              &raw_removals, expected_checksum));
  V4StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(store.hash_prefix_map_->WriteToDisk(sb_file_format));

  HashPrefixMapView prefix_map = store.hash_prefix_map_->view();
  // The size is 2 since we reserve space anyway.
  EXPECT_EQ(2u, prefix_map.size());
  EXPECT_EQ("22223333", prefix_map.at(4));
  EXPECT_EQ("1111133333", prefix_map.at(5));
}

TEST_F(V4StoreTest, TestMergeUpdatesRemovesWhenOldHasDifferentSizes) {
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "222233334444", &prefix_map_old));
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "aaaaabbbbb", &prefix_map_old));
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "1111133333", &prefix_map_additions));

  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  RepeatedField<int32_t> raw_removals;
  // old_store: ["2222", "3333", 4444", "aaaaa", "bbbbb"]
  raw_removals.Add(3);  // Removes "aaaaa"
  std::string expected_checksum = std::string(
      "\xA7OG\x9D\x83.\x9D-f\x8A\xE\x8B\r&\x19"
      "6\xE3\xF0\xEFTi\xA7\x5\xEA\xF7"
      "ej,\xA8\x9D\xAD\x91",
      crypto::hash::kSha256Size);
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            store.MergeUpdate(PrefixMapToView(prefix_map_old),
                              PrefixMapToView(prefix_map_additions),
                              &raw_removals, expected_checksum));
  V4StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(store.hash_prefix_map_->WriteToDisk(sb_file_format));

  HashPrefixMapView prefix_map = store.hash_prefix_map_->view();
  // The size is 2 since we reserve space anyway.
  EXPECT_EQ(2u, prefix_map.size());
  EXPECT_EQ("222233334444", prefix_map.at(4));
  EXPECT_EQ("1111133333bbbbb", prefix_map.at(5));
}

TEST_F(V4StoreTest, TestMergeUpdatesRemovesMultipleAcrossDifferentSizes) {
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "22223333aaaa", &prefix_map_old));
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "3333344444bbbbb", &prefix_map_old));
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "11111", &prefix_map_additions));

  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  RepeatedField<int32_t> raw_removals;
  // old_store: ["2222", "3333", "33333", "44444", "aaaa", "bbbbb"]
  raw_removals.Add(1);  // Removes "3333"
  raw_removals.Add(3);  // Removes "44444"
  std::string expected_checksum = std::string(
      "!D\xB7&L\xA7&G0\x85\xB4"
      "E\xDD\x10\"\x9A\xCA\xF1"
      "3^\x83w\xBBL\x19n\xAD\xBDM\x9D"
      "b\x9F",
      crypto::hash::kSha256Size);
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            store.MergeUpdate(PrefixMapToView(prefix_map_old),
                              PrefixMapToView(prefix_map_additions),
                              &raw_removals, expected_checksum));
  V4StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(store.hash_prefix_map_->WriteToDisk(sb_file_format));

  HashPrefixMapView prefix_map = store.hash_prefix_map_->view();
  // The size is 2 since we reserve space anyway.
  EXPECT_EQ(2u, prefix_map.size());
  EXPECT_EQ("2222aaaa", prefix_map.at(4));
  EXPECT_EQ("1111133333bbbbb", prefix_map.at(5));
}

TEST_F(V4StoreTest, TestReadFullResponseWithValidHashPrefixMap) {
  V4Store write_store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                      /*is_eligible_for_migration=*/true,
                      /*is_extensions_blocklist=*/false);
  write_store.hash_prefix_map_->Append(4, "00000abc");
  write_store.hash_prefix_map_->Append(5, "00000abcde");
  write_store.state_ = "test_client_state";
  EXPECT_FALSE(base::PathExists(write_store.store_path_));
  EXPECT_EQ(WRITE_SUCCESS, write_store.WriteToDisk(Checksum()));
  EXPECT_TRUE(base::PathExists(write_store.store_path_));

  V4Store read_store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                     /*is_eligible_for_migration=*/true,
                     /*is_extensions_blocklist=*/false);
  EXPECT_EQ(READ_SUCCESS, read_store.ReadFromDisk());
  EXPECT_EQ("test_client_state", read_store.state_);
  ASSERT_EQ(2u, read_store.hash_prefix_map_->view().size());
  EXPECT_EQ("00000abc", read_store.hash_prefix_map_->view()[4]);
  EXPECT_EQ("00000abcde", read_store.hash_prefix_map_->view()[5]);
  EXPECT_EQ(write_store.file_size_, read_store.file_size_);
}

// This tests fails to read the prefix map from the disk because the file on
// disk is invalid. The hash prefixes string is 6 bytes long, but the prefix
// size is 5 so the parser isn't able to split the hash prefixes list
// completely.
TEST_F(V4StoreTest, TestReadFullResponseWithInvalidHashPrefixMap) {
  // Manually create an invalid store on disk
  V4StoreFileFormat file_format;
  file_format.set_magic_number(0x600D71FE);
  file_format.set_version_number(9);
  ListUpdateResponse* list_update_response =
      file_format.mutable_list_update_response();
  list_update_response->set_new_client_state("test_client_state");
  list_update_response->set_platform_type(LINUX_PLATFORM);
  list_update_response->set_response_type(ListUpdateResponse::FULL_UPDATE);
  HashFile* hash_file = file_format.add_hash_files();
  hash_file->set_prefix_size(5);
  hash_file->set_extension("foo");
  hash_file->set_file_size(6);
  base::WriteFile(store_path_, file_format.SerializeAsString());
  base::WriteFile(store_path_.AddExtensionASCII("foo"), "abcdef");

  V4Store read_store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                     /*is_eligible_for_migration=*/true,
                     /*is_extensions_blocklist=*/false);
  EXPECT_EQ(HASH_PREFIX_MAP_GENERATION_FAILURE, read_store.ReadFromDisk());
  EXPECT_TRUE(read_store.state_.empty());
  EXPECT_TRUE(read_store.hash_prefix_map_->view().empty());
  EXPECT_EQ(0, read_store.file_size_);
}

TEST_F(V4StoreTest, TestWriteFullResponseWithInvalidHashPrefixMap) {
  V4Store write_store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                      /*is_eligible_for_migration=*/true,
                      /*is_extensions_blocklist=*/false);
  write_store.hash_prefix_map_->Append(5, "abcdef");
  write_store.state_ = "test_client_state";
  EXPECT_FALSE(base::PathExists(write_store.store_path_));
  EXPECT_EQ(UNEXPECTED_WRITE_FAILURE, write_store.WriteToDisk(Checksum()));
  EXPECT_FALSE(base::PathExists(write_store.store_path_));
}

TEST_F(V4StoreTest, TestHashPrefixExistsAtTheBeginning) {
  HashPrefixMap map(store_path_);
  map.Append(5, "abcdebbbbbccccc");
  V4StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(map.WriteToDisk(sb_file_format));
  HashPrefixStr hash_prefix = "abcde";
  EXPECT_EQ(map.GetMatchingHashPrefix(hash_prefix), hash_prefix);
}

TEST_F(V4StoreTest, TestHashPrefixExistsInTheMiddle) {
  HashPrefixMap map(store_path_);
  map.Append(5, "abcdebbbbbccccc");
  V4StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(map.WriteToDisk(sb_file_format));
  HashPrefixStr hash_prefix = "bbbbb";
  EXPECT_EQ(map.GetMatchingHashPrefix(hash_prefix), hash_prefix);
}

TEST_F(V4StoreTest, TestHashPrefixExistsAtTheEnd) {
  HashPrefixMap map(store_path_);
  map.Append(5, "abcdebbbbbccccc");
  V4StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(map.WriteToDisk(sb_file_format));
  HashPrefixStr hash_prefix = "ccccc";
  EXPECT_EQ(map.GetMatchingHashPrefix(hash_prefix), hash_prefix);
}

TEST_F(V4StoreTest, TestHashPrefixExistsAtTheBeginningOfEven) {
  HashPrefixMap map(store_path_);
  map.Append(5, "abcdebbbbb");
  V4StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(map.WriteToDisk(sb_file_format));
  HashPrefixStr hash_prefix = "abcde";
  EXPECT_EQ(map.GetMatchingHashPrefix(hash_prefix), hash_prefix);
}

TEST_F(V4StoreTest, TestHashPrefixExistsAtTheEndOfEven) {
  HashPrefixMap map(store_path_);
  map.Append(5, "abcdebbbbb");
  V4StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(map.WriteToDisk(sb_file_format));
  HashPrefixStr hash_prefix = "bbbbb";
  EXPECT_EQ(map.GetMatchingHashPrefix(hash_prefix), hash_prefix);
}

TEST_F(V4StoreTest, TestHashPrefixDoesNotExistInConcatenatedList) {
  HashPrefixMap map(store_path_);
  map.Append(5, "abcdebbbbb");
  V4StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(map.WriteToDisk(sb_file_format));
  HashPrefixStr hash_prefix = "bbbbc";
  EXPECT_EQ(map.GetMatchingHashPrefix(hash_prefix), "");
}

TEST_F(V4StoreTest, TestFullHashExistsInMapWithSingleSize) {
  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  store.hash_prefix_map_->Append(
      32, "0111222233334444555566667777888811112222333344445555666677778888");
  V4StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(store.hash_prefix_map_->WriteToDisk(sb_file_format));
  FullHashStr full_hash = "11112222333344445555666677778888";
  EXPECT_EQ("11112222333344445555666677778888",
            store.GetMatchingHashPrefix(full_hash));
}

TEST_F(V4StoreTest, TestFullHashExistsInMapWithDifferentSizes) {
  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  store.hash_prefix_map_->Append(4, "22223333aaaa");
  store.hash_prefix_map_->Append(32, "11112222333344445555666677778888");
  V4StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(store.hash_prefix_map_->WriteToDisk(sb_file_format));

  FullHashStr full_hash = "11112222333344445555666677778888";
  EXPECT_EQ("11112222333344445555666677778888",
            store.GetMatchingHashPrefix(full_hash));
}

TEST_F(V4StoreTest, TestHashPrefixExistsInMapWithSingleSize) {
  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  store.hash_prefix_map_->Append(4, "22223333aaaa");
  V4StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(store.hash_prefix_map_->WriteToDisk(sb_file_format));
  FullHashStr full_hash = "22222222222222222222222222222222";
  EXPECT_EQ("2222", store.GetMatchingHashPrefix(full_hash));
}

TEST_F(V4StoreTest, TestHashPrefixExistsInMapWithDifferentSizes) {
  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  store.hash_prefix_map_->Append(4, "22223333aaaa");
  store.hash_prefix_map_->Append(5, "11111hhhhh");
  V4StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(store.hash_prefix_map_->WriteToDisk(sb_file_format));

  FullHashStr full_hash = "22222222222222222222222222222222";
  EXPECT_EQ("2222", store.GetMatchingHashPrefix(full_hash));
}

TEST_F(V4StoreTest, TestHashPrefixDoesNotExistInMapWithDifferentSizes) {
  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  store.hash_prefix_map_->Append(4, "3333aaaa");
  store.hash_prefix_map_->Append(5, "11111hhhhh");
  V4StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(store.hash_prefix_map_->WriteToDisk(sb_file_format));

  FullHashStr full_hash = "22222222222222222222222222222222";
  EXPECT_TRUE(store.GetMatchingHashPrefix(full_hash).empty());
}

TEST_F(V4StoreTest, GetMatchingHashPrefixSize32Or21) {
  HashPrefixStr prefix = "0123";
  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  store.hash_prefix_map_->Append(4, prefix);
  V4StoreFileFormat file_format;
  SBStoreFileFormat sb_file_format(&file_format);
  EXPECT_TRUE(store.hash_prefix_map_->WriteToDisk(sb_file_format));

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

TEST_F(V4StoreTest, TestAdditionsWithRiceEncodingFailsWithInvalidInput) {
  RepeatedPtrField<ThreatEntrySet> additions;
  ThreatEntrySet* addition = additions.Add();
  addition->set_compression_type(RICE);
  addition->mutable_rice_hashes()->set_num_entries(-1);
  std::unordered_map<PrefixSize, HashPrefixes> additions_map;
  EXPECT_EQ(RICE_DECODING_FAILURE,
            V4Store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                    /*is_eligible_for_migration=*/true,
                    /*is_extensions_blocklist=*/false)
                .UpdateHashPrefixMapFromAdditions("V4Metric", additions,
                                                  &additions_map));
}

TEST_F(V4StoreTest,
       TestAdditionsWithRiceEncodingFailsWithInvalidCompressionType) {
  RepeatedPtrField<ThreatEntrySet> additions;
  ThreatEntrySet* addition = additions.Add();
  addition->set_compression_type(COMPRESSION_TYPE_UNSPECIFIED);
  std::unordered_map<PrefixSize, HashPrefixes> additions_map;
  EXPECT_EQ(UNEXPECTED_COMPRESSION_TYPE_ADDITIONS_FAILURE,
            V4Store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                    /*is_eligible_for_migration=*/true,
                    /*is_extensions_blocklist=*/false)
                .UpdateHashPrefixMapFromAdditions("V4Metric", additions,
                                                  &additions_map));
}

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
  std::unordered_map<PrefixSize, HashPrefixes> additions_map;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                    /*is_eligible_for_migration=*/true,
                    /*is_extensions_blocklist=*/false)
                .UpdateHashPrefixMapFromAdditions("V4Metric", additions,
                                                  &additions_map));
  EXPECT_EQ(1u, additions_map.size());
  EXPECT_EQ(std::string("\x5\0\0\0\fL\x93\xADV\x7F\xF6o\xCEo1\x81", 16),
            additions_map[4]);
}

TEST_F(V4StoreTest, TestRemovalsWithRiceEncodingSucceeds) {
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "1111abcdefgh", &prefix_map_old));
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "22222bcdef", &prefix_map_additions));

  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  std::string expected_checksum = std::string(
      "\xA5\x8B\xCAsD\xC7\xF9\xCE\xD2\xF4\x4="
      "\xB2\"\x82\x1A\xC1\xB8\x1F\x10\r\v\x9A\x93\xFD\xE1\xB8"
      "B\x1Eh\xF7\xB4",
      crypto::hash::kSha256Size);
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            store.MergeUpdate(PrefixMapToView(prefix_map_old),
                              PrefixMapToView(prefix_map_additions), nullptr,
                              expected_checksum));
  EXPECT_FALSE(store.HasValidData());  // Never actually read from disk.

  V4StoreFileFormat file_format;
  store.WriteToDisk(&file_format);

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

  base::RunLoop run_loop;
  UpdatedStoreReadyCallback store_ready_callback =
      base::BindOnce(&V4StoreTest::UpdatedStoreReady, base::Unretained(this),
                     &run_loop, true /* expect_store */);
  auto sb_response = std::make_unique<SBUpdateResponse>();
  sb_response->v4_response = std::move(lur);
  store.ApplyUpdate(std::move(sb_response), task_runner(),
                    std::move(store_ready_callback));
  EXPECT_TRUE(base::PathExists(store.store_path_));

  run_loop.Run();

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

  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "2222", &prefix_map_old));
  EXPECT_EQ(CHECKSUM_MISMATCH_FAILURE,
            V4Store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                    /*is_eligible_for_migration=*/true,
                    /*is_extensions_blocklist=*/false)
                .MergeUpdate(PrefixMapToView(prefix_map_old),
                             HashPrefixMapView(), nullptr, "aawc"));
}

TEST_F(V4StoreTest, TestChecksumErrorOnStartup) {
  // Proof of checksum match using python:
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

  ListUpdateResponse list_update_response;
  list_update_response.set_new_client_state("test_client_state");
  list_update_response.set_platform_type(LINUX_PLATFORM);
  list_update_response.set_response_type(ListUpdateResponse::FULL_UPDATE);
  list_update_response.mutable_checksum()->set_sha256(expected_checksum);

  V4StoreFileFormat file_format;
  auto* hash_file = file_format.add_hash_files();
  hash_file->set_prefix_size(5);
  hash_file->set_extension("foo");
  hash_file->set_file_size(5);

  // First the case of checksum not matching after reading from disk.
  {
    // "abcdf" does not match the expected checksum.
    base::WriteFile(store_path_.AddExtensionASCII("foo"), "abcdf");
    WriteFileFormatProtoToFile(&file_format, 0x600D71FE, 9,
                               &list_update_response);
    V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                  /*is_eligible_for_migration=*/true,
                  /*is_extensions_blocklist=*/false);
    EXPECT_TRUE(store.expected_checksum_.empty());
    EXPECT_EQ(READ_SUCCESS, store.ReadFromDisk());
    EXPECT_TRUE(!store.expected_checksum_.empty());
    EXPECT_EQ("test_client_state", store.state());
    EXPECT_EQ(85, store.file_size_);

    EXPECT_FALSE(store.VerifyChecksum());
  }

  // Now the case of checksum matching after reading from disk.
  {
    // "abcde" does match the expected checksum.
    base::WriteFile(store_path_.AddExtensionASCII("foo"), "abcde");
    WriteFileFormatProtoToFile(&file_format, 0x600D71FE, 9,
                               &list_update_response);
    V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                  /*is_eligible_for_migration=*/true,
                  /*is_extensions_blocklist=*/false);
    EXPECT_TRUE(store.expected_checksum_.empty());
    EXPECT_EQ(READ_SUCCESS, store.ReadFromDisk());
    EXPECT_TRUE(!store.expected_checksum_.empty());
    EXPECT_EQ("test_client_state", store.state());
    EXPECT_EQ(85, store.file_size_);

    EXPECT_TRUE(store.VerifyChecksum());
  }
}

TEST_F(V4StoreTest, WriteToDiskFails) {
  // Pass the directory name as file name so that when the code tries to rename
  // the temp store file to |store_path_| it fails.
  EXPECT_EQ(UNABLE_TO_RENAME_FAILURE,
            V4Store(task_runner(), temp_dir_.GetPath(), /*v5_prefix_size=*/4,
                    /*is_eligible_for_migration=*/true,
                    /*is_extensions_blocklist=*/false)
                .WriteToDisk(Checksum()));

  // Give a location that isn't writable, even for the tmp file.
  base::FilePath non_writable_dir =
      temp_dir_.GetPath()
          .Append(FILE_PATH_LITERAL("nonexistent_dir"))
          .Append(FILE_PATH_LITERAL("some.store"));
  EXPECT_EQ(UNEXPECTED_BYTES_WRITTEN_FAILURE,
            V4Store(task_runner(), non_writable_dir, /*v5_prefix_size=*/4,
                    /*is_eligible_for_migration=*/true,
                    /*is_extensions_blocklist=*/false)
                .WriteToDisk(Checksum()));
}

TEST_F(V4StoreTest, FullUpdateFailsChecksumSynchronously) {
  base::HistogramTester histogram_tester;
  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  base::RunLoop run_loop;
  UpdatedStoreReadyCallback store_ready_callback =
      base::BindOnce(&V4StoreTest::UpdatedStoreReady, base::Unretained(this),
                     &run_loop, false /* expect_store */);
  EXPECT_FALSE(base::PathExists(store.store_path_));
  EXPECT_FALSE(store.HasValidData());  // Never actually read from disk.

  // Now create a response with invalid checksum.
  std::unique_ptr<ListUpdateResponse> lur(new ListUpdateResponse);
  lur->set_response_type(ListUpdateResponse::FULL_UPDATE);
  lur->mutable_checksum()->set_sha256(
      std::string(crypto::hash::kSha256Size, 0));
  auto sb_response = std::make_unique<SBUpdateResponse>();
  sb_response->v4_response = std::move(lur);
  store.ApplyUpdate(std::move(sb_response), task_runner(),
                    std::move(store_ready_callback));
  // The update should fail synchronously and not create a store file.
  EXPECT_FALSE(base::PathExists(store.store_path_));

  run_loop.Run();

  // Ensure that the file is still not created.
  EXPECT_FALSE(base::PathExists(store.store_path_));
  EXPECT_FALSE(updated_store_);

  EXPECT_EQ(store.last_apply_update_result_, CHECKSUM_MISMATCH_FAILURE);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.V4ProcessUpdate.UpdateType",
                                      V4Store::ApplyUpdateType::kFull, 1);
}

TEST_F(V4StoreTest, ApplyUpdateFailsWithInvalidResponseType) {
  base::HistogramTester histogram_tester;
  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  base::RunLoop run_loop;
  UpdatedStoreReadyCallback store_ready_callback =
      base::BindOnce(&V4StoreTest::UpdatedStoreReady, base::Unretained(this),
                     &run_loop, false /* expect_store */);
  EXPECT_FALSE(base::PathExists(store.store_path_));
  EXPECT_FALSE(store.HasValidData());  // Never actually read from disk.

  // Now create a response with an invalid response type.
  std::unique_ptr<ListUpdateResponse> lur(new ListUpdateResponse);
  lur->set_response_type(ListUpdateResponse::RESPONSE_TYPE_UNSPECIFIED);
  auto sb_response = std::make_unique<SBUpdateResponse>();
  sb_response->v4_response = std::move(lur);
  store.ApplyUpdate(std::move(sb_response), task_runner(),
                    std::move(store_ready_callback));
  // The update should fail synchronously and not create a store file.
  EXPECT_FALSE(base::PathExists(store.store_path_));

  run_loop.Run();

  // Ensure that the file is still not created.
  EXPECT_FALSE(base::PathExists(store.store_path_));
  EXPECT_FALSE(updated_store_);

  EXPECT_EQ(store.last_apply_update_result_, UNEXPECTED_RESPONSE_TYPE_FAILURE);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.V4ProcessUpdate.UpdateType",
                                      V4Store::ApplyUpdateType::kInvalid, 1);
}

TEST_F(V4StoreTest, ApplyUpdateRemovalsFailsWithInvalidCompressionType) {
  base::HistogramTester histogram_tester;
  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  base::RunLoop run_loop;
  UpdatedStoreReadyCallback store_ready_callback =
      base::BindOnce(&V4StoreTest::UpdatedStoreReady, base::Unretained(this),
                     &run_loop, false /* expect_store */);
  EXPECT_FALSE(base::PathExists(store.store_path_));
  EXPECT_FALSE(store.HasValidData());  // Never actually read from disk.

  // Now create a response with an invalid removals compression type.
  std::unique_ptr<ListUpdateResponse> lur(new ListUpdateResponse);
  lur->set_response_type(ListUpdateResponse::PARTIAL_UPDATE);
  ThreatEntrySet* removal = lur->add_removals();
  removal->set_compression_type(COMPRESSION_TYPE_UNSPECIFIED);
  auto sb_response = std::make_unique<SBUpdateResponse>();
  sb_response->v4_response = std::move(lur);
  store.ApplyUpdate(std::move(sb_response), task_runner(),
                    std::move(store_ready_callback));
  // The update should fail synchronously and not create a store file.
  EXPECT_FALSE(base::PathExists(store.store_path_));

  run_loop.Run();

  // Ensure that the file is still not created.
  EXPECT_FALSE(base::PathExists(store.store_path_));
  EXPECT_FALSE(updated_store_);

  EXPECT_EQ(store.last_apply_update_result_,
            UNEXPECTED_COMPRESSION_TYPE_REMOVALS_FAILURE);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.V4ProcessUpdate.UpdateType",
                                      V4Store::ApplyUpdateType::kPartial, 1);
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

  base::WriteFile(HashPrefixMap::GetPath(store_path_, "foo"), "abcde");

  V4StoreFileFormat file_format;
  auto* hash_file = file_format.add_hash_files();
  hash_file->set_prefix_size(5);
  hash_file->set_extension("foo");
  hash_file->set_file_size(5);

  WriteFileFormatProtoToFile(&file_format, 0x600D71FE, 9,
                             &list_update_response);
  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
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
  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);

  EXPECT_EQ(HASH_PREFIX_MAP_GENERATION_FAILURE, store.ReadFromDisk());
}

TEST_F(V4StoreTest, MigrateToMmap) {
  const std::string kFullHash = "abcdefghijklmnopqrstu";
  const std::string kHash = "abcde";
  V4Store write_store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                      /*is_eligible_for_migration=*/true,
                      /*is_extensions_blocklist=*/false);
  write_store.state_ = "test_client_state";
  write_store.hash_prefix_map_->Append(5, kHash);
  EXPECT_EQ(WRITE_SUCCESS, write_store.WriteToDisk(Checksum()));

  // Make sure an in-memory store can read correctly.
  V4Store in_memory_store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                          /*is_eligible_for_migration=*/true,
                          /*is_extensions_blocklist=*/false);
  EXPECT_EQ(READ_SUCCESS, in_memory_store.ReadFromDisk());
  EXPECT_EQ("test_client_state", in_memory_store.state());
  EXPECT_EQ(in_memory_store.hash_prefix_map_->view()[5], kHash);
  EXPECT_EQ(in_memory_store.GetMatchingHashPrefix(kFullHash), kHash);

  // Migrate to a mmap store.
  V4Store mmap_store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                     /*is_eligible_for_migration=*/true,
                     /*is_extensions_blocklist=*/false);
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
      HashPrefixMap::GetPath(store_path_,
                             file_format.hash_files(0).extension()),
      &contents));
  EXPECT_EQ(contents, kHash);
  EXPECT_EQ(mmap_store.file_size(),
            static_cast<int64_t>(proto_contents.size() + kHash.size()));

  // Reading again should not migrate.
  base::Time last_modified = GetLastModifiedTime(store_path_);
  V4Store mmap_store2(task_runner(), store_path_, /*v5_prefix_size=*/4,
                      /*is_eligible_for_migration=*/true,
                      /*is_extensions_blocklist=*/false);
  EXPECT_EQ(READ_SUCCESS, mmap_store2.ReadFromDisk());
  EXPECT_EQ(GetLastModifiedTime(store_path_), last_modified);
  EXPECT_EQ(mmap_store2.GetMatchingHashPrefix(kFullHash), kHash);
}

TEST_F(V4StoreTest, CleanUpOldFiles) {
  base::FilePath old_hashes_path = HashPrefixMap::GetPath(store_path_, "foo");
  base::WriteFile(old_hashes_path, "abcde");

  base::FilePath other_path = temp_dir_.GetPath().AppendASCII("SomePath");
  base::WriteFile(other_path, "stuff");

  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(WRITE_SUCCESS, store.WriteToDisk(Checksum()));

  EXPECT_FALSE(base::PathExists(old_hashes_path));
  EXPECT_TRUE(base::PathExists(other_path));
}

TEST_F(V4StoreTest, FileSizeIncludesHashFiles) {
  V4Store write_store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                      /*is_eligible_for_migration=*/true,
                      /*is_extensions_blocklist=*/false);
  write_store.hash_prefix_map_->Append(4, "abcd");
  EXPECT_EQ(WRITE_SUCCESS, write_store.WriteToDisk(Checksum()));

  int64_t original_file_size = write_store.file_size();

  static_cast<HashPrefixMap*>(write_store.hash_prefix_map_.get())
      ->ClearAndWaitForTesting();
  write_store.Reset();
  write_store.hash_prefix_map_->Append(4, "abcd");
  write_store.hash_prefix_map_->Append(4, "efgh");
  EXPECT_EQ(WRITE_SUCCESS, write_store.WriteToDisk(Checksum()));
  EXPECT_EQ(write_store.file_size(), original_file_size + 4);

  V4Store read_store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                     /*is_eligible_for_migration=*/true,
                     /*is_extensions_blocklist=*/false);
  EXPECT_EQ(READ_SUCCESS, read_store.ReadFromDisk());
  EXPECT_EQ(read_store.file_size(), original_file_size + 4);
}

TEST_F(V4StoreTest, MergeUpdatesWithHashPrefixMap) {
  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_old;
  prefix_map_old[4] = "abcdefgh";
  prefix_map_old[5] = "54321abcde";

  std::unordered_map<PrefixSize, HashPrefixes> prefix_map_additions;
  prefix_map_additions[4] = "----1111bbbb";
  prefix_map_additions[5] = "22222bcdef";

  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
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
      crypto::hash::kSha256Size);
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            store.MergeUpdate(PrefixMapToView(prefix_map_old),
                              PrefixMapToView(prefix_map_additions), nullptr,
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
}

TEST_F(V4StoreTest, PreMmapMigrationFileFormatFails) {
  ListUpdateResponse list_update_response;
  list_update_response.set_new_client_state("test_client_state");
  list_update_response.set_platform_type(LINUX_PLATFORM);
  list_update_response.set_response_type(ListUpdateResponse::FULL_UPDATE);
  ThreatEntrySet* additions = list_update_response.add_additions();
  additions->set_compression_type(RAW);
  additions->mutable_raw_hashes()->set_prefix_size(5);
  additions->mutable_raw_hashes()->set_raw_hashes("abcde");

  WriteFileFormatProtoToFile(0x600D71FE, 9, &list_update_response);

  V4Store read_store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                     /*is_eligible_for_migration=*/true,
                     /*is_extensions_blocklist=*/false);
  EXPECT_EQ(PRE_MMAP_MIGRATION_FILE_FORMAT_FAILURE, read_store.ReadFromDisk());
  EXPECT_TRUE(read_store.state().empty());
  EXPECT_TRUE(read_store.hash_prefix_map_->view().empty());
}

TEST_F(V4StoreTest, TestMigrationFailureOpenFailureV5) {
  // Create a directory at v5_store_path_ to force read failure.
  ASSERT_TRUE(base::CreateDirectory(v5_store_path_));

  base::HistogramTester histograms;
  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5_TO_V4_MIGRATION_FAILURE, store.ReadFromDisk());

  // Verify migration result is kReadV5Failed.
  histograms.ExpectUniqueSample("SafeBrowsing.V4Store.V5ToV4MigrationResult",
                                V5ToV4MigrationResult::kReadV5Failed,
                                /*expected_bucket_count=*/1);

  // Verify V5 Read Failure Reason is kFileOpenFailure (2).
  histograms.ExpectUniqueSample(
      "SafeBrowsing.V4Store.V5ToV4Migration.V5ReadFailureReason",
      V5StoreReadResult::kFileOpenFailure,
      /*expected_bucket_count=*/1);

  // Verify V5 files are wiped.
  EXPECT_FALSE(base::PathExists(v5_store_path_));
}

TEST_F(V4StoreTest, TestMigrationFailureEmptyV5) {
  // Write empty file.
  base::CloseFile(base::OpenFile(v5_store_path_, "wb+"));

  base::HistogramTester histograms;
  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5_TO_V4_MIGRATION_FAILURE, store.ReadFromDisk());

  // Verify migration result is kReadV5Failed.
  histograms.ExpectUniqueSample("SafeBrowsing.V4Store.V5ToV4MigrationResult",
                                V5ToV4MigrationResult::kReadV5Failed,
                                /*expected_bucket_count=*/1);

  // Verify V5 Read Failure Reason is kFileEmptyFailure (3).
  histograms.ExpectUniqueSample(
      "SafeBrowsing.V4Store.V5ToV4Migration.V5ReadFailureReason",
      V5StoreReadResult::kFileEmptyFailure,
      /*expected_bucket_count=*/1);

  // Verify V5 files are wiped.
  EXPECT_FALSE(base::PathExists(v5_store_path_));
}

TEST_F(V4StoreTest, TestMigrationFailureCorruptedV5) {
  // Write invalid proto contents.
  base::WriteFile(v5_store_path_, "Chromium");

  base::HistogramTester histograms;
  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5_TO_V4_MIGRATION_FAILURE, store.ReadFromDisk());

  // Verify migration result is kReadV5Failed.
  histograms.ExpectUniqueSample("SafeBrowsing.V4Store.V5ToV4MigrationResult",
                                V5ToV4MigrationResult::kReadV5Failed,
                                /*expected_bucket_count=*/1);

  // Verify V5 Read Failure Reason is kProtoParsingFailure (4).
  histograms.ExpectUniqueSample(
      "SafeBrowsing.V4Store.V5ToV4Migration.V5ReadFailureReason",
      V5StoreReadResult::kProtoParsingFailure,
      /*expected_bucket_count=*/1);

  // Verify V5 files are wiped.
  EXPECT_FALSE(base::PathExists(v5_store_path_));
}

TEST_F(V4StoreTest, TestMigrationFailureIncompatibleVersionV5) {
  // Write V5 file with version 2 (incompatible).
  WriteV5FileFormatProtoToFile(/*magic=*/0x600D71FE, /*file_version=*/2);

  base::HistogramTester histograms;
  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5_TO_V4_MIGRATION_FAILURE, store.ReadFromDisk());

  // Verify migration result is kReadV5Failed.
  histograms.ExpectUniqueSample("SafeBrowsing.V4Store.V5ToV4MigrationResult",
                                V5ToV4MigrationResult::kReadV5Failed,
                                /*expected_bucket_count=*/1);

  // Verify V5 Read Failure Reason is kFileVersionIncompatibleFailure (6).
  histograms.ExpectUniqueSample(
      "SafeBrowsing.V4Store.V5ToV4Migration.V5ReadFailureReason",
      V5StoreReadResult::kFileVersionIncompatibleFailure,
      /*expected_bucket_count=*/1);

  // Verify V5 files are wiped.
  EXPECT_FALSE(base::PathExists(v5_store_path_));
}

TEST_F(V4StoreTest, TestMigrationFailureMissingDetailsV5) {
  // Write V5 file without ListDetails.
  WriteV5FileFormatProtoToFile(/*magic=*/0x600D71FE, /*file_version=*/10);

  base::HistogramTester histograms;
  V4Store store(task_runner(), store_path_, /*v5_prefix_size=*/4,
                /*is_eligible_for_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5_TO_V4_MIGRATION_FAILURE, store.ReadFromDisk());

  // Verify migration result is kReadV5Failed.
  histograms.ExpectUniqueSample("SafeBrowsing.V4Store.V5ToV4MigrationResult",
                                V5ToV4MigrationResult::kReadV5Failed,
                                /*expected_bucket_count=*/1);

  // Verify V5 Read Failure Reason is kHashPrefixInfoMissingFailure (7).
  histograms.ExpectUniqueSample(
      "SafeBrowsing.V4Store.V5ToV4Migration.V5ReadFailureReason",
      V5StoreReadResult::kHashPrefixInfoMissingFailure,
      /*expected_bucket_count=*/1);

  // Verify V5 files are wiped.
  EXPECT_FALSE(base::PathExists(v5_store_path_));
}

}  // namespace safe_browsing
