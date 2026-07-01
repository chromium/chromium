// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v5_store.h"

#include <optional>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/safe_browsing/core/browser/db/v4_store.pb.h"
#include "components/safe_browsing/core/common/proto/v5_store.pb.h"
#include "crypto/hash.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"

namespace safe_browsing {

class V5StoreTest : public PlatformTest {
 public:
  V5StoreTest() = default;

  void SetUp() override {
    PlatformTest::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    store_path_ = temp_dir_.GetPath().AppendASCII("V5StoreTest.store");
    v4_store_path_ = temp_dir_.GetPath().AppendASCII("V4StoreTest.store");
    DVLOG(1) << "store_path_: " << store_path_.value();
    DVLOG(1) << "v4_store_path_: " << v4_store_path_.value();
  }

  void TearDown() override {
    base::DeleteFile(store_path_);
    base::DeleteFile(v4_store_path_);
    PlatformTest::TearDown();
  }

  void WriteFileFormatProtoToFile(uint32_t magic,
                                  uint32_t file_version = 0,
                                  ListDetails* details = nullptr) {
    V5StoreFileFormat file_format;
    WriteFileFormatProtoToFile(&file_format, magic, file_version, details);
  }

  void WriteFileFormatProtoToFile(V5StoreFileFormat* file_format,
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
    base::WriteFile(store_path_, file_format_string);
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner() {
    return base::SequencedTaskRunner::GetCurrentDefault();
  }

  V4ToV5MigrationResult MigrateFromV4(V5Store& store,
                                      const base::FilePath& v4_store_path) {
    return store.MigrateFromV4(v4_store_path);
  }

  V5StoreReadResult ReadFromDisk(V5Store& store) {
    return store.ReadFromDisk();
  }

  const HashPrefixList& GetHashPrefixList(const V5Store& store) {
    return *store.hash_prefix_list_;
  }

  int64_t GetFileSize(const V5Store& store) { return store.file_size_; }

  std::string GetExpectedChecksum(const V5Store& store) {
    return store.expected_checksum_;
  }

  std::string ExtensionV4IdToV5Hash(std::string_view v4_id) {
    return SBStore::ExtensionV4IdToV5Hash(v4_id);
  }

  void WriteV4FileFormatProtoToFile(
      const base::FilePath& path,
      uint32_t magic,
      uint32_t file_version,
      std::optional<std::string> client_state,
      std::optional<std::string> checksum_sha256,
      const std::vector<std::pair<std::string, uint64_t>>& hash_files,
      PrefixSize prefix_size = 4) {
    V4StoreFileFormat file_format;
    file_format.set_magic_number(magic);
    file_format.set_version_number(file_version);

    ListUpdateResponse* response = file_format.mutable_list_update_response();
    if (client_state.has_value()) {
      response->set_new_client_state(client_state.value());
    }
    if (checksum_sha256.has_value()) {
      response->mutable_checksum()->set_sha256(checksum_sha256.value());
    }
    response->set_response_type(ListUpdateResponse::FULL_UPDATE);

    for (const auto& [ext, size] : hash_files) {
      HashFile* hash_file = file_format.add_hash_files();
      hash_file->set_prefix_size(prefix_size);
      hash_file->set_extension(ext);
      hash_file->set_file_size(size);
    }

    std::string file_format_string;
    file_format.SerializeToString(&file_format_string);
    base::WriteFile(path, file_format_string);
  }

  void RunExtensionMigrationFailureTest(
      uint64_t v4_hash_file_size,
      base::OnceClosure setup_failure_condition,
      ConvertExtensionBlocklistV4ToV5Result expected_result,
      bool expect_v4_hash_file_deleted,
      base::OnceClosure teardown_cleanup = base::OnceClosure()) {
    std::vector<std::pair<std::string, uint64_t>> v4_hash_files = {
        {"32_foo", v4_hash_file_size}};
    WriteV4FileFormatProtoToFile(
        v4_store_path_, /*magic=*/0x600D71FE, /*file_version=*/9,
        /*client_state=*/"v4_version", /*checksum_sha256=*/std::nullopt,
        v4_hash_files, /*prefix_size=*/32);

    if (!setup_failure_condition.is_null()) {
      std::move(setup_failure_condition).Run();
    }

    V5Store store(task_runner(), store_path_, /*prefix_size=*/16,
                  v4_store_path_,
                  /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                  /*is_extensions_blocklist=*/true);
    EXPECT_EQ(V5StoreReadResult::kV4ToV5MigrationFailure, ReadFromDisk(store));

    histogram_tester_.ExpectUniqueSample(
        "SafeBrowsing.V5Store.ConvertExtensionBlocklistV4ToV5Result",
        expected_result, 1);
    histogram_tester_.ExpectUniqueSample(
        "SafeBrowsing.V5Store.V4ToV5MigrationResult",
        V4ToV5MigrationResult::kExtensionBlocklistMigrationFailed, 1);

    EXPECT_FALSE(base::PathExists(v4_store_path_));
    EXPECT_FALSE(base::PathExists(store_path_));
    if (expect_v4_hash_file_deleted) {
      EXPECT_FALSE(
          base::PathExists(v4_store_path_.AddExtensionASCII("32_foo")));
    }

    if (!teardown_cleanup.is_null()) {
      std::move(teardown_cleanup).Run();
    }
  }

  void RunExtensionMigrationChecksumTest(
      std::optional<std::string> override_checksum,
      bool expect_success) {
    std::vector<std::pair<std::string, uint64_t>> v4_hash_files = {
        {"32_foo", 64}};

    std::string v4_data =
        "aapbdbdomjkkjkaonfhkkikfgjllcleb"
        "aapbdbdomjkkjkaonfhkkikfgjllclec";
    base::WriteFile(v4_store_path_.AddExtensionASCII("32_foo"), v4_data);

    std::optional<std::string> checksum_to_write;
    if (override_checksum.has_value()) {
      checksum_to_write = override_checksum.value();
    } else {
      std::array<uint8_t, crypto::hash::kSha256Size> v4_checksum;
      crypto::hash::Hash(crypto::hash::HashKind::kSha256,
                         base::as_byte_span(v4_data), v4_checksum);
      checksum_to_write = std::string(
          reinterpret_cast<char*>(v4_checksum.data()), v4_checksum.size());
    }

    WriteV4FileFormatProtoToFile(v4_store_path_, 0x600D71FE, 9, "v4_version",
                                 checksum_to_write, v4_hash_files, 32);

    V5Store store(task_runner(), store_path_, /*prefix_size=*/16,
                  v4_store_path_,
                  /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                  /*is_extensions_blocklist=*/true);

    V5StoreReadResult expected_read_result =
        expect_success ? V5StoreReadResult::kReadSuccess
                       : V5StoreReadResult::kV4ToV5MigrationFailure;
    EXPECT_EQ(expected_read_result, ReadFromDisk(store));

    if (expect_success) {
      // Verify V5 files created.
      EXPECT_TRUE(base::PathExists(store_path_));
      EXPECT_TRUE(base::PathExists(store_path_.AddExtensionASCII("foo")));
      EXPECT_FALSE(base::PathExists(store_path_.AddExtensionASCII("32_foo")));

      // Verify V4 files deleted.
      EXPECT_FALSE(base::PathExists(v4_store_path_));
      EXPECT_FALSE(
          base::PathExists(v4_store_path_.AddExtensionASCII("32_foo")));

      // Verify data.
      EXPECT_EQ("v4_version", store.version());
      std::string expected_v5_data;
      expected_v5_data.append(
          ExtensionV4IdToV5Hash("aapbdbdomjkkjkaonfhkkikfgjllcleb"));
      expected_v5_data.append(
          ExtensionV4IdToV5Hash("aapbdbdomjkkjkaonfhkkikfgjllclec"));
      EXPECT_EQ(GetHashPrefixList(store).view().at(16), expected_v5_data);

      // Verify checksum.
      if (override_checksum != "") {
        std::array<uint8_t, crypto::hash::kSha256Size> v5_checksum;
        crypto::hash::Hash(crypto::hash::HashKind::kSha256,
                           base::as_byte_span(expected_v5_data), v5_checksum);
        EXPECT_EQ(std::string(reinterpret_cast<char*>(v5_checksum.data()),
                              v5_checksum.size()),
                  GetExpectedChecksum(store));
      } else {
        EXPECT_TRUE(GetExpectedChecksum(store).empty());
      }
    } else {
      // Verify files are wiped on failure.
      EXPECT_FALSE(base::PathExists(v4_store_path_));
      EXPECT_FALSE(
          base::PathExists(v4_store_path_.AddExtensionASCII("32_foo")));
      EXPECT_FALSE(base::PathExists(store_path_));
    }

    V4ToV5MigrationResult expected_migration_result =
        expect_success
            ? V4ToV5MigrationResult::kV4ToV5MigrationSucceeded
            : V4ToV5MigrationResult::kExtensionBlocklistMigrationFailed;
    ConvertExtensionBlocklistV4ToV5Result expected_conversion_result =
        expect_success
            ? ConvertExtensionBlocklistV4ToV5Result::kSuccess
            : ConvertExtensionBlocklistV4ToV5Result::kV4ChecksumMismatch;

    histogram_tester_.ExpectUniqueSample(
        "SafeBrowsing.V5Store.V4ToV5MigrationResult", expected_migration_result,
        1);
    histogram_tester_.ExpectUniqueSample(
        "SafeBrowsing.V5Store.ConvertExtensionBlocklistV4ToV5Result",
        expected_conversion_result, 1);
  }

  void ExpectChecksumHistograms(V5ApplyUpdateResult expected_result) {
    histogram_tester_.ExpectUniqueSample(
        "SafeBrowsing.V5ReadFromDisk.ApplyUpdate.Result", expected_result, 1);
    histogram_tester_.ExpectUniqueSample(
        "SafeBrowsing.V5ReadFromDisk.ApplyUpdate.Result.V5StoreTest",
        expected_result, 1);
    histogram_tester_.ExpectTotalCount(
        "SafeBrowsing.V5ReadFromDisk.VerifyChecksumDuration", 1);
    histogram_tester_.ExpectTotalCount(
        "SafeBrowsing.SBReadFromDisk.VerifyChecksumDuration", 1);
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath store_path_;
  base::FilePath v4_store_path_;
  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
};

TEST_F(V5StoreTest, TestReadFromEmptyFile) {
  base::CloseFile(base::OpenFile(store_path_, "wb+"));

  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5StoreReadResult::kFileEmptyFailure, ReadFromDisk(store));
}

TEST_F(V5StoreTest, TestReadFromAbsentFile) {
  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5StoreReadResult::kFileOpenFailure, ReadFromDisk(store));
}

TEST_F(V5StoreTest, TestReadFromInvalidContentsFile) {
  const char kInvalidContents[] = "Chromium";
  base::WriteFile(store_path_, kInvalidContents);
  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5StoreReadResult::kProtoParsingFailure, ReadFromDisk(store));
}

TEST_F(V5StoreTest, TestReadFromFileWithUnknownProto) {
  Checksum checksum;
  checksum.set_sha256("checksum");
  std::string checksum_string;
  checksum.SerializeToString(&checksum_string);
  base::WriteFile(store_path_, checksum_string);

  // Even though we wrote a completely different proto to file, the proto
  // parsing method does not fail. This shows the importance of a magic number.
  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5StoreReadResult::kUnexpectedMagicNumberFailure,
            ReadFromDisk(store));
}

TEST_F(V5StoreTest, TestReadFromUnexpectedMagicFile) {
  WriteFileFormatProtoToFile(111);
  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5StoreReadResult::kUnexpectedMagicNumberFailure,
            ReadFromDisk(store));
}

TEST_F(V5StoreTest, TestReadFromLowVersionFile) {
  WriteFileFormatProtoToFile(0x600D71FE, 2);
  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5StoreReadResult::kFileVersionIncompatibleFailure,
            ReadFromDisk(store));
}

TEST_F(V5StoreTest, TestReadFromNoHashPrefixInfoFile) {
  WriteFileFormatProtoToFile(0x600D71FE, 10);
  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5StoreReadResult::kHashPrefixInfoMissingFailure,
            ReadFromDisk(store));
}

TEST_F(V5StoreTest, TestReadFromNoHashPrefixesFile) {
  ListDetails list_details;
  list_details.set_version("test_version");
  auto hash = crypto::hash::Sha256(base::span<const uint8_t>());
  list_details.mutable_checksum()->set_sha256(
      std::string(hash.begin(), hash.end()));
  WriteFileFormatProtoToFile(0x600D71FE, 10, &list_details);
  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  store.Initialize();
  EXPECT_TRUE(store.HasValidData());
  EXPECT_TRUE(store.VerifyChecksum());
  EXPECT_TRUE(GetHashPrefixList(store).view().empty());
  EXPECT_EQ(60, GetFileSize(store));

  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5ReadFromDisk.ApplyUpdate.Result",
      V5ApplyUpdateResult::kSuccess, 1);
  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5ReadFromDisk.ApplyUpdate.Result.V5StoreTest",
      V5ApplyUpdateResult::kSuccess, 1);
}

TEST_F(V5StoreTest, TestReadFromInvalidHashPrefixList) {
  // Manually create an invalid store on disk
  V5StoreFileFormat file_format;
  file_format.set_magic_number(0x600D71FE);
  file_format.set_file_version(10);
  ListDetails* list_details = file_format.mutable_list_details();
  list_details->set_version("test_client_version");
  V5HashFile* hash_file = list_details->mutable_hash_file();
  hash_file->set_extension("foo");
  // Set file size to 6, which is not a multiple of 4.
  hash_file->set_file_size(6);
  // Write the file format and hash file to disk.
  base::WriteFile(store_path_, file_format.SerializeAsString());
  base::WriteFile(store_path_.AddExtensionASCII("foo"), "abcdef");
  // Set the prefix size to 4. This will cause a failure in the read.
  V5Store read_store(task_runner(), store_path_, 4, v4_store_path_,
                     /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                     /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5StoreReadResult::kHashPrefixListGenerationFailure,
            ReadFromDisk(read_store));
  EXPECT_TRUE(read_store.version().empty());
  EXPECT_TRUE(GetHashPrefixList(read_store).view().empty());
  EXPECT_EQ(0, GetFileSize(read_store));

  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5ReadFromDisk.ApplyUpdate.Result",
      V5ApplyUpdateResult::kFileSizeNotMultipleOfPrefixSize, 1);
  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5ReadFromDisk.ApplyUpdate.Result.V5StoreTest",
      V5ApplyUpdateResult::kFileSizeNotMultipleOfPrefixSize, 1);
}

TEST_F(V5StoreTest, TestReadWithMissingHashFile) {
  V5StoreFileFormat file_format;
  ListDetails list_details;
  list_details.set_version("test_client_version");
  V5HashFile* hash_file = list_details.mutable_hash_file();
  hash_file->set_extension("foo");
  hash_file->set_file_size(4);
  // Write only the file format to disk. The hash file is missing.
  WriteFileFormatProtoToFile(&file_format, 0x600D71FE, 10, &list_details);
  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);

  EXPECT_EQ(V5StoreReadResult::kHashPrefixListGenerationFailure,
            ReadFromDisk(store));

  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5ReadFromDisk.ApplyUpdate.Result",
      V5ApplyUpdateResult::kMmapFailure, 1);
  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5ReadFromDisk.ApplyUpdate.Result.V5StoreTest",
      V5ApplyUpdateResult::kMmapFailure, 1);
}

TEST_F(V5StoreTest, TestInitializeSucceeds) {
  ListDetails list_details;
  list_details.set_version("test_version");
  WriteFileFormatProtoToFile(0x600D71FE, 10, &list_details);
  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  store.Initialize();
  EXPECT_TRUE(store.HasValidData());

  histogram_tester_.ExpectUniqueSample("SafeBrowsing.V5StoreRead.Result",
                                       V5StoreReadResult::kReadSuccess, 1);
  histogram_tester_.ExpectUniqueSample("SafeBrowsing.V5Store.IsStoreValid",
                                       true, 1);
  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5Store.IsStoreValid.V5StoreTest", true, 1);
  histogram_tester_.ExpectUniqueSample("SafeBrowsing.SBStoreRead.Success", true,
                                       1);
  histogram_tester_.ExpectUniqueSample("SafeBrowsing.SBStore.IsStoreValid",
                                       true, 1);
  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.SBStore.IsStoreValid.V5StoreTest", true, 1);
}

TEST_F(V5StoreTest, TestInitializeSucceedsWithV5Suffix) {
  base::FilePath v5_store_path =
      temp_dir_.GetPath().AppendASCII("V5StoreTest_v5.store");

  ListDetails list_details;
  list_details.set_version("test_version");

  // Temporarily swap store_path_ to use the helper
  base::FilePath original_store_path = store_path_;
  store_path_ = v5_store_path;
  WriteFileFormatProtoToFile(0x600D71FE, 10, &list_details);
  store_path_ = original_store_path;  // restore

  V5Store store(task_runner(), v5_store_path, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  store.Initialize();
  EXPECT_TRUE(store.HasValidData());

  histogram_tester_.ExpectUniqueSample("SafeBrowsing.V5StoreRead.Result",
                                       V5StoreReadResult::kReadSuccess, 1);
  histogram_tester_.ExpectUniqueSample("SafeBrowsing.V5Store.IsStoreValid",
                                       true, 1);
  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5Store.IsStoreValid.V5StoreTest_v5", true, 1);
  histogram_tester_.ExpectUniqueSample("SafeBrowsing.SBStoreRead.Success", true,
                                       1);
  histogram_tester_.ExpectUniqueSample("SafeBrowsing.SBStore.IsStoreValid",
                                       true, 1);
  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.SBStore.IsStoreValid.V5StoreTest", true, 1);

  base::DeleteFile(v5_store_path);
}

TEST_F(V5StoreTest, TestInitializeFails) {
  // No file on disk, so Initialize will fail.
  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  store.Initialize();
  EXPECT_FALSE(store.HasValidData());

  histogram_tester_.ExpectUniqueSample("SafeBrowsing.V5StoreRead.Result",
                                       V5StoreReadResult::kFileOpenFailure, 1);
  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5Store.V4ToV5MigrationResult",
      V4ToV5MigrationResult::kV4StoreNotFound, 1);
  histogram_tester_.ExpectUniqueSample("SafeBrowsing.V5Store.IsStoreValid",
                                       false, 1);
  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5Store.IsStoreValid.V5StoreTest", false, 1);
  histogram_tester_.ExpectUniqueSample("SafeBrowsing.SBStoreRead.Success",
                                       false, 1);
  histogram_tester_.ExpectUniqueSample("SafeBrowsing.SBStore.IsStoreValid",
                                       false, 1);
  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.SBStore.IsStoreValid.V5StoreTest", false, 1);
}

TEST_F(V5StoreTest, TestReadFromDiskDoesNotSetValidData) {
  ListDetails list_details;
  list_details.set_version("test_version");
  WriteFileFormatProtoToFile(0x600D71FE, 10, &list_details);
  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5StoreReadResult::kReadSuccess, ReadFromDisk(store));
  // Only `Initialize()` sets the `has_valid_data_` property.
  EXPECT_FALSE(store.HasValidData());

  histogram_tester_.ExpectUniqueSample("SafeBrowsing.V5Store.IsStoreValid",
                                       false, 1);
  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5Store.IsStoreValid.V5StoreTest", false, 1);
  histogram_tester_.ExpectUniqueSample("SafeBrowsing.SBStore.IsStoreValid",
                                       false, 1);
  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.SBStore.IsStoreValid.V5StoreTest", false, 1);
  histogram_tester_.ExpectTotalCount("SafeBrowsing.V5StoreRead.Result", 0);
  histogram_tester_.ExpectTotalCount("SafeBrowsing.SBStoreRead.Success", 0);
}

TEST_F(V5StoreTest, TestHasValidDataLoggingHeuristic) {
  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);

  // First call should log.
  store.HasValidData();
  histogram_tester_.ExpectUniqueSample("SafeBrowsing.V5Store.IsStoreValid",
                                       false, 1);
  histogram_tester_.ExpectUniqueSample("SafeBrowsing.SBStore.IsStoreValid",
                                       false, 1);

  // Calls 2 to 256 should NOT log.
  for (int i = 2; i <= 256; ++i) {
    store.HasValidData();
  }
  histogram_tester_.ExpectUniqueSample("SafeBrowsing.V5Store.IsStoreValid",
                                       false, 1);
  histogram_tester_.ExpectUniqueSample("SafeBrowsing.SBStore.IsStoreValid",
                                       false, 1);

  // Next call should log again (total 2 samples).
  store.HasValidData();
  histogram_tester_.ExpectUniqueSample("SafeBrowsing.V5Store.IsStoreValid",
                                       false, 2);
  histogram_tester_.ExpectUniqueSample("SafeBrowsing.SBStore.IsStoreValid",
                                       false, 2);
}

TEST_F(V5StoreTest, TestReadFromNoVersionFile) {
  ListDetails list_details;
  // `version` is omitted.
  WriteFileFormatProtoToFile(0x600D71FE, 10, &list_details);
  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5StoreReadResult::kReadSuccess, ReadFromDisk(store));
  EXPECT_TRUE(store.version().empty());
}

TEST_F(V5StoreTest, TestReadWithValidChecksum) {
  ListDetails list_details;
  list_details.set_version("test_version");
  list_details.mutable_checksum()->set_sha256(
      "test_checksum_value_32_bytes____");
  WriteFileFormatProtoToFile(0x600D71FE, 10, &list_details);
  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5StoreReadResult::kReadSuccess, ReadFromDisk(store));
  EXPECT_EQ("test_checksum_value_32_bytes____", GetExpectedChecksum(store));
}

TEST_F(V5StoreTest, TestReadWithMissingSha256) {
  ListDetails list_details;
  list_details.set_version("test_version");
  list_details.mutable_checksum();  // checksum is present but empty
  WriteFileFormatProtoToFile(0x600D71FE, 10, &list_details);
  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5StoreReadResult::kReadSuccess, ReadFromDisk(store));
  EXPECT_TRUE(GetExpectedChecksum(store).empty());
}

TEST_F(V5StoreTest, TestReadWithValidHashPrefixList) {
  V5StoreFileFormat file_format;
  file_format.set_magic_number(0x600D71FE);
  file_format.set_file_version(10);
  ListDetails* list_details = file_format.mutable_list_details();
  list_details->set_version("test_version");
  V5HashFile* hash_file = list_details->mutable_hash_file();
  hash_file->set_extension("foo");
  hash_file->set_file_size(4);

  std::string data = "abcd";
  std::array<uint8_t, crypto::hash::kSha256Size> checksum;
  crypto::hash::Hash(crypto::hash::HashKind::kSha256, base::as_byte_span(data),
                     checksum);
  list_details->mutable_checksum()->set_sha256(
      std::string(reinterpret_cast<char*>(checksum.data()), checksum.size()));

  // Write main proto.
  base::WriteFile(store_path_, file_format.SerializeAsString());
  // Write valid hash file (4 bytes).
  base::WriteFile(store_path_.AddExtensionASCII("foo"), data);

  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  store.Initialize();
  EXPECT_TRUE(store.HasValidData());
  EXPECT_EQ("test_version", store.version());
  EXPECT_EQ(GetHashPrefixList(store).view().at(4), "abcd");
  EXPECT_EQ(base::checked_cast<int64_t>(file_format.ByteSizeLong() + 4),
            GetFileSize(store));

  EXPECT_TRUE(store.VerifyChecksum());

  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5ReadFromDisk.ApplyUpdate.Result",
      V5ApplyUpdateResult::kSuccess, 1);
  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5ReadFromDisk.ApplyUpdate.Result.V5StoreTest",
      V5ApplyUpdateResult::kSuccess, 1);
}

TEST_F(V5StoreTest, TestMigrationAlreadyV5) {
  ListDetails list_details;
  list_details.set_version("test_version");
  WriteFileFormatProtoToFile(0x600D71FE, 10, &list_details);

  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5StoreReadResult::kReadSuccess, ReadFromDisk(store));

  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5Store.V4ToV5MigrationResult",
      V4ToV5MigrationResult::kDiskAlreadyV5, 1);
}

TEST_F(V5StoreTest, TestMigrationNotEligible_WipeSucceeds) {
  std::vector<std::pair<std::string, uint64_t>> v4_hash_files = {{"4_foo", 4}};
  WriteV4FileFormatProtoToFile(v4_store_path_, 0x600D71FE, 9, "v4_version",
                               "v4_checksum", v4_hash_files);
  base::WriteFile(v4_store_path_.AddExtensionASCII("4_foo"), "abcd");

  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/false,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5StoreReadResult::kV4ToV5MigrationWipedSuccessfully,
            ReadFromDisk(store));

  // Verify V4 files are wiped.
  EXPECT_FALSE(base::PathExists(v4_store_path_));
  EXPECT_FALSE(base::PathExists(v4_store_path_.AddExtensionASCII("4_foo")));

  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5Store.V4ToV5MigrationResult",
      V4ToV5MigrationResult::kStoreIneligibleWipeSucceeded, 1);
  histogram_tester_.ExpectTotalCount(
      "SafeBrowsing.V5Store.V4ToV5Migration.TimeTaken.V5StoreTest", 1);
}

TEST_F(V5StoreTest, TestMigrationNotEligible_WipeFails) {
  std::vector<std::pair<std::string, uint64_t>> v4_hash_files = {{"4_foo", 4}};
  WriteV4FileFormatProtoToFile(v4_store_path_, 0x600D71FE, 9, "v4_version",
                               "v4_checksum", v4_hash_files);
  base::WriteFile(v4_store_path_.AddExtensionASCII("4_foo"), "abcd");

  // Force wipe to fail by making the V4 store path a non-empty directory.
  base::DeleteFile(v4_store_path_);
  ASSERT_TRUE(base::CreateDirectory(v4_store_path_));
  ASSERT_TRUE(base::WriteFile(v4_store_path_.AppendASCII("dummy"), "dummy"));

  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/false,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5StoreReadResult::kV4ToV5MigrationFailure, ReadFromDisk(store));

  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5Store.V4ToV5MigrationResult",
      V4ToV5MigrationResult::kStoreIneligibleWipeFailed, 1);

  // Cleanup dummy directory.
  base::DeletePathRecursively(v4_store_path_);
}

TEST_F(V5StoreTest,
       TestMigrationNotEligible_WipeHashFileFails_WipeStoreFileSucceeds) {
  std::vector<std::pair<std::string, uint64_t>> v4_hash_files = {{"4_foo", 4}};
  WriteV4FileFormatProtoToFile(v4_store_path_, 0x600D71FE, 9, "v4_version",
                               "v4_checksum", v4_hash_files);

  // Force hash file wipe to fail by making it a non-empty directory.
  base::FilePath hash_file_path = v4_store_path_.AddExtensionASCII("4_foo");
  ASSERT_TRUE(base::CreateDirectory(hash_file_path));
  ASSERT_TRUE(base::WriteFile(hash_file_path.AppendASCII("dummy"), "dummy"));

  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/false,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5StoreReadResult::kV4ToV5MigrationFailure, ReadFromDisk(store));

  // The V4 hash file still exists because the wipe failed to delete it.
  EXPECT_TRUE(base::PathExists(hash_file_path));
  // But in spite of that, the V4 store file was still able to be deleted.
  EXPECT_FALSE(base::PathExists(v4_store_path_));

  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5Store.V4ToV5MigrationResult",
      V4ToV5MigrationResult::kStoreIneligibleWipeFailed, 1);

  // Cleanup dummy directory.
  base::DeletePathRecursively(hash_file_path);
}

TEST_F(V5StoreTest, TestMigrationV4NotFound) {
  // V5 doesn't exist, V4 doesn't exist.
  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5StoreReadResult::kFileOpenFailure, ReadFromDisk(store));

  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5Store.V4ToV5MigrationResult",
      V4ToV5MigrationResult::kV4StoreNotFound, 1);
}

TEST_F(V5StoreTest, TestMigrationSuccess) {
  // Write valid V4 store and hash file with "4_" prefix in extension.
  std::vector<std::pair<std::string, uint64_t>> v4_hash_files = {{"4_foo", 4}};
  WriteV4FileFormatProtoToFile(v4_store_path_, 0x600D71FE, 9, "v4_version",
                               "v4_checksum", v4_hash_files);
  base::WriteFile(v4_store_path_.AddExtensionASCII("4_foo"), "abcd");

  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  // Verify ReadFromDisk performs the migration and succeeds.
  EXPECT_EQ(V5StoreReadResult::kReadSuccess, ReadFromDisk(store));

  // Verify V5 files created and correct (extension without "4_" prefix).
  EXPECT_TRUE(base::PathExists(store_path_));
  EXPECT_TRUE(base::PathExists(store_path_.AddExtensionASCII("foo")));
  EXPECT_FALSE(base::PathExists(store_path_.AddExtensionASCII("4_foo")));

  // Verify V4 store file and hash file deleted.
  EXPECT_FALSE(base::PathExists(v4_store_path_));
  EXPECT_FALSE(base::PathExists(v4_store_path_.AddExtensionASCII("4_foo")));

  // Verify we can read it now.
  EXPECT_EQ("v4_version", store.version());
  EXPECT_EQ(GetHashPrefixList(store).view().at(4), "abcd");

  // Verify UMA logging.
  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5Store.V4ToV5MigrationResult",
      V4ToV5MigrationResult::kV4ToV5MigrationSucceeded, 1);
  histogram_tester_.ExpectTotalCount(
      "SafeBrowsing.V5Store.V4ToV5Migration.TimeTaken.V5StoreTest", 1);
}

TEST_F(V5StoreTest, TestMigrationProtoParsingFailure) {
  // Write corrupted V4 file.
  base::WriteFile(v4_store_path_, "CorruptedProtoContent");

  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5StoreReadResult::kV4ToV5MigrationFailure, ReadFromDisk(store));

  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5Store.V4ToV5MigrationResult",
      V4ToV5MigrationResult::kReadV4Failed, 1);
  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5Store.V4ToV5Migration.V4ReadFailureReason",
      PROTO_PARSING_FAILURE, 1);
}

TEST_F(V5StoreTest, TestMigrationUnexpectedMagic) {
  WriteV4FileFormatProtoToFile(v4_store_path_, 111, 9, "v4_version",
                               "v4_checksum", {});

  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5StoreReadResult::kV4ToV5MigrationFailure, ReadFromDisk(store));

  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5Store.V4ToV5MigrationResult",
      V4ToV5MigrationResult::kReadV4Failed, 1);
  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5Store.V4ToV5Migration.V4ReadFailureReason",
      UNEXPECTED_MAGIC_NUMBER_FAILURE, 1);
}

TEST_F(V5StoreTest, TestMigrationVersionIncompatible) {
  WriteV4FileFormatProtoToFile(v4_store_path_, 0x600D71FE, 8, "v4_version",
                               "v4_checksum", {});

  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5StoreReadResult::kV4ToV5MigrationFailure, ReadFromDisk(store));

  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5Store.V4ToV5MigrationResult",
      V4ToV5MigrationResult::kReadV4Failed, 1);
  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5Store.V4ToV5Migration.V4ReadFailureReason",
      FILE_VERSION_INCOMPATIBLE_FAILURE, 1);
}

TEST_F(V5StoreTest, TestMigrationMultipleHashFilesNotSupported) {
  std::vector<std::pair<std::string, uint64_t>> v4_hash_files = {{"foo", 4},
                                                                 {"bar", 4}};
  WriteV4FileFormatProtoToFile(v4_store_path_, 0x600D71FE, 9, "v4_version",
                               "v4_checksum", v4_hash_files);

  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5StoreReadResult::kV4ToV5MigrationFailure, ReadFromDisk(store));

  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5Store.V4ToV5MigrationResult",
      V4ToV5MigrationResult::kMultipleHashFilesFailure, 1);
}

TEST_F(V5StoreTest, TestMigrationPrefixSizeMismatch) {
  std::vector<std::pair<std::string, uint64_t>> v4_hash_files = {{"foo", 4}};
  WriteV4FileFormatProtoToFile(v4_store_path_, 0x600D71FE, 9, "v4_version",
                               "v4_checksum", v4_hash_files);
  base::WriteFile(v4_store_path_.AddExtensionASCII("foo"), "abcd");

  V5Store store(task_runner(), store_path_, 8, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5StoreReadResult::kV4ToV5MigrationFailure, ReadFromDisk(store));

  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5Store.V4ToV5MigrationResult",
      V4ToV5MigrationResult::kPrefixSizeMismatchFailure, 1);
}

TEST_F(V5StoreTest, TestMigrationHashFileMissing) {
  std::vector<std::pair<std::string, uint64_t>> v4_hash_files = {{"foo", 4}};
  WriteV4FileFormatProtoToFile(v4_store_path_, 0x600D71FE, 9, "v4_version",
                               "v4_checksum", v4_hash_files);

  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5StoreReadResult::kV4ToV5MigrationFailure, ReadFromDisk(store));

  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5Store.V4ToV5MigrationResult",
      V4ToV5MigrationResult::kHashFileMissingFailure, 1);
}

TEST_F(V5StoreTest, TestMigrationWriteV5Failure) {
  std::vector<std::pair<std::string, uint64_t>> v4_hash_files = {{"4_foo", 4}};
  WriteV4FileFormatProtoToFile(v4_store_path_, 0x600D71FE, 9, "v4_version",
                               "v4_checksum", v4_hash_files);
  base::WriteFile(v4_store_path_.AddExtensionASCII("4_foo"), "abcd");

  // Create a directory at temp_store_path to force base::WriteFile to fail.
  base::FilePath temp_store_path = store_path_.AddExtensionASCII("tmp");
  ASSERT_TRUE(base::CreateDirectory(temp_store_path));

  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5StoreReadResult::kV4ToV5MigrationFailure, ReadFromDisk(store));

  // Verify V4 files and partial V5 files are deleted.
  EXPECT_FALSE(base::PathExists(v4_store_path_.AddExtensionASCII("4_foo")));
  EXPECT_FALSE(base::PathExists(store_path_.AddExtensionASCII("foo")));
  EXPECT_FALSE(base::PathExists(v4_store_path_));

  // Cleanup symlink.
  base::DeleteFile(temp_store_path);

  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5Store.V4ToV5MigrationResult",
      V4ToV5MigrationResult::kWriteV5FileFailure, 1);
}

TEST_F(V5StoreTest, TestMigrationV4Empty) {
  base::CloseFile(base::OpenFile(v4_store_path_, "wb+"));

  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5StoreReadResult::kV4ToV5MigrationFailure, ReadFromDisk(store));

  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5Store.V4ToV5MigrationResult",
      V4ToV5MigrationResult::kReadV4Failed, 1);
  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5Store.V4ToV5Migration.V4ReadFailureReason",
      FILE_EMPTY_FAILURE, 1);
}

TEST_F(V5StoreTest, TestMigrationNoHashFiles) {
  WriteV4FileFormatProtoToFile(v4_store_path_, 0x600D71FE, 9, "v4_version",
                               "v4_checksum", {});

  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5StoreReadResult::kReadSuccess, ReadFromDisk(store));

  EXPECT_TRUE(base::PathExists(store_path_));
  EXPECT_FALSE(base::PathExists(v4_store_path_));

  EXPECT_EQ("v4_version", store.version());
  EXPECT_TRUE(GetHashPrefixList(store).view().empty());

  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5Store.V4ToV5MigrationResult",
      V4ToV5MigrationResult::kV4ToV5MigrationSucceeded, 1);
}

TEST_F(V5StoreTest, TestMigrationFailureNoUnderscore) {
  std::vector<std::pair<std::string, uint64_t>> v4_hash_files = {{"foo", 4}};
  WriteV4FileFormatProtoToFile(v4_store_path_, 0x600D71FE, 9, "v4_version",
                               "v4_checksum", v4_hash_files);
  base::WriteFile(v4_store_path_.AddExtensionASCII("foo"), "abcd");

  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5StoreReadResult::kV4ToV5MigrationFailure, ReadFromDisk(store));

  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5Store.V4ToV5MigrationResult",
      V4ToV5MigrationResult::kExtensionParsingFailure, 1);
}

TEST_F(V5StoreTest, TestMigrationFailureEmptyV5Extension) {
  std::vector<std::pair<std::string, uint64_t>> v4_hash_files = {{"foo_", 4}};
  WriteV4FileFormatProtoToFile(v4_store_path_, 0x600D71FE, 9, "v4_version",
                               "v4_checksum", v4_hash_files);
  base::WriteFile(v4_store_path_.AddExtensionASCII("foo_"), "abcd");

  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5StoreReadResult::kV4ToV5MigrationFailure, ReadFromDisk(store));

  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5Store.V4ToV5MigrationResult",
      V4ToV5MigrationResult::kExtensionParsingFailure, 1);
}

TEST_F(V5StoreTest, TestMigrationRenameFailure) {
  std::vector<std::pair<std::string, uint64_t>> v4_hash_files = {{"4_foo", 4}};
  WriteV4FileFormatProtoToFile(v4_store_path_, 0x600D71FE, 9, "v4_version",
                               "v4_checksum", v4_hash_files);
  base::WriteFile(v4_store_path_.AddExtensionASCII("4_foo"), "abcd");

  // Create a directory at the destination hash file path to force base::Move to
  // fail.
  base::FilePath v5_hash_file_path = store_path_.AddExtensionASCII("foo");
  ASSERT_TRUE(base::CreateDirectory(v5_hash_file_path));

  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5StoreReadResult::kV4ToV5MigrationFailure, ReadFromDisk(store));

  // Verify V4 files are deleted.
  EXPECT_FALSE(base::PathExists(v4_store_path_));
  EXPECT_FALSE(base::PathExists(v4_store_path_.AddExtensionASCII("4_foo")));
  EXPECT_FALSE(base::PathExists(store_path_));

  // Cleanup directory.
  base::DeletePathRecursively(v5_hash_file_path);

  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5Store.V4ToV5MigrationResult",
      V4ToV5MigrationResult::kRenameHashFileFailure, 1);
}

TEST_F(V5StoreTest, TestMigrationMissingOptionalFields) {
  std::vector<std::pair<std::string, uint64_t>> v4_hash_files = {{"4_foo", 4}};
  WriteV4FileFormatProtoToFile(v4_store_path_, 0x600D71FE, 9, std::nullopt,
                               std::nullopt, v4_hash_files);
  base::WriteFile(v4_store_path_.AddExtensionASCII("4_foo"), "abcd");

  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  EXPECT_EQ(V5StoreReadResult::kReadSuccess, ReadFromDisk(store));

  EXPECT_TRUE(base::PathExists(store_path_));
  EXPECT_TRUE(base::PathExists(store_path_.AddExtensionASCII("foo")));

  // Verify we can read it, and optional fields are missing.
  EXPECT_TRUE(store.version().empty());
  EXPECT_TRUE(GetExpectedChecksum(store).empty());

  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5Store.V4ToV5MigrationResult",
      V4ToV5MigrationResult::kV4ToV5MigrationSucceeded, 1);
}

TEST_F(V5StoreTest, TestMigrationSuccessButReadFailure) {
  std::vector<std::pair<std::string, uint64_t>> v4_hash_files = {{"4_foo", 4}};
  WriteV4FileFormatProtoToFile(v4_store_path_, 0x600D71FE, 9, "v4_version",
                               "v4_checksum", v4_hash_files);
  // Write corrupted hash file (only 2 bytes, expected 4).
  base::WriteFile(v4_store_path_.AddExtensionASCII("4_foo"), "ab");

  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  // Reading it fails because the hash file is corrupted (size mismatch),
  // even though migration itself succeeded.
  EXPECT_EQ(V5StoreReadResult::kHashPrefixListGenerationFailure,
            ReadFromDisk(store));

  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5Store.V4ToV5MigrationResult",
      V4ToV5MigrationResult::kV4ToV5MigrationSucceeded, 1);
}

TEST_F(V5StoreTest, TestMigrationRenameV5StoreFileFailure) {
  std::vector<std::pair<std::string, uint64_t>> v4_hash_files = {{"4_foo", 4}};
  WriteV4FileFormatProtoToFile(v4_store_path_, 0x600D71FE, 9, "v4_version",
                               "v4_checksum", v4_hash_files);
  base::WriteFile(v4_store_path_.AddExtensionASCII("4_foo"), "abcd");

  // Create a directory at store_path_ to force base::Move to fail.
  ASSERT_TRUE(base::CreateDirectory(store_path_));

  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);

  // Call MigrateFromV4 directly to bypass PathExists check in
  // AttemptV4ToV5Migration so this test is able to trigger this case.
  EXPECT_EQ(V4ToV5MigrationResult::kRenameV5StoreFileFailure,
            MigrateFromV4(store, v4_store_path_));

  // Verify V4 files are deleted.
  EXPECT_FALSE(base::PathExists(v4_store_path_));
  EXPECT_FALSE(base::PathExists(v4_store_path_.AddExtensionASCII("4_foo")));

  // Verify temp file is deleted.
  base::FilePath temp_store_path = store_path_.AddExtensionASCII("tmp");
  EXPECT_FALSE(base::PathExists(temp_store_path));

  // Cleanup directory.
  base::DeletePathRecursively(store_path_);
}

TEST_F(V5StoreTest, TestExtensionMigrationSuccess) {
  // Write valid V4 store and hash file with 32-byte IDs.
  std::vector<std::pair<std::string, uint64_t>> v4_hash_files = {
      {"32_foo", 64}};
  WriteV4FileFormatProtoToFile(v4_store_path_, 0x600D71FE, 9, "v4_version",
                               std::nullopt, v4_hash_files, 32);

  // 2 valid extension IDs (32 chars 'a'-'p').
  std::string v4_data =
      "aapbdbdomjkkjkaonfhkkikfgjllcleb"
      "aapbdbdomjkkjkaonfhkkikfgjllclec";
  base::WriteFile(v4_store_path_.AddExtensionASCII("32_foo"), v4_data);

  // V5Store expected prefix size is 16.
  V5Store store(task_runner(), store_path_, 16, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/true);
  EXPECT_EQ(V5StoreReadResult::kReadSuccess, ReadFromDisk(store));

  // Verify V5 files created and correct.
  EXPECT_TRUE(base::PathExists(store_path_));
  EXPECT_TRUE(base::PathExists(store_path_.AddExtensionASCII("foo")));
  EXPECT_FALSE(base::PathExists(store_path_.AddExtensionASCII("32_foo")));

  // Verify V4 files deleted.
  EXPECT_FALSE(base::PathExists(v4_store_path_));
  EXPECT_FALSE(base::PathExists(v4_store_path_.AddExtensionASCII("32_foo")));

  // Verify read data.
  EXPECT_EQ("v4_version", store.version());

  // Expected V5 hashes (16 bytes each).
  std::string expected_v5_data;
  expected_v5_data.append(
      ExtensionV4IdToV5Hash("aapbdbdomjkkjkaonfhkkikfgjllcleb"));
  expected_v5_data.append(
      ExtensionV4IdToV5Hash("aapbdbdomjkkjkaonfhkkikfgjllclec"));
  EXPECT_EQ(GetHashPrefixList(store).view().at(16), expected_v5_data);

  // Verify checksum is not written because the source had no checksum.
  EXPECT_TRUE(GetExpectedChecksum(store).empty());

  // Verify UMA.
  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5Store.ConvertExtensionBlocklistV4ToV5Result",
      ConvertExtensionBlocklistV4ToV5Result::kSuccess, 1);
  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5Store.V4ToV5MigrationResult",
      V4ToV5MigrationResult::kV4ToV5MigrationSucceeded, 1);
}

TEST_F(V5StoreTest, TestExtensionMigrationFailureInvalidId) {
  RunExtensionMigrationFailureTest(
      /*v4_hash_file_size=*/64,
      /*setup_failure_condition=*/
      base::BindOnce(
          [](const base::FilePath& path) {
            // 'z' is not a valid extension ID character.
            std::string v4_data =
                "aapbdbdomjkkjkaonfhkkikfgjllclez"
                "aapbdbdomjkkjkaonfhkkikfgjllclec";
            base::WriteFile(path.AddExtensionASCII("32_foo"), v4_data);
          },
          v4_store_path_),
      /*expected_result=*/
      ConvertExtensionBlocklistV4ToV5Result::kInvalidExtensionId,
      /*expect_v4_hash_file_deleted=*/true);
}

TEST_F(V5StoreTest, TestExtensionMigrationFailureReadV4) {
  base::FilePath hash_file_path = v4_store_path_.AddExtensionASCII("32_foo");
  RunExtensionMigrationFailureTest(
      /*v4_hash_file_size=*/64,
      /*setup_failure_condition=*/
      base::BindOnce(
          [](const base::FilePath& path) {
            // Make it fail to try to read the v4 file.
            ASSERT_TRUE(base::CreateDirectory(path));
          },
          hash_file_path),
      /*expected_result=*/ConvertExtensionBlocklistV4ToV5Result::kReadV4Failed,
      /*expect_v4_hash_file_deleted=*/false,
      /*teardown_cleanup=*/
      base::BindOnce(
          [](const base::FilePath& path) { base::DeletePathRecursively(path); },
          hash_file_path));
}

TEST_F(V5StoreTest, TestExtensionMigrationFailureInvalidFileSize) {
  RunExtensionMigrationFailureTest(
      /*v4_hash_file_size=*/10,
      /*setup_failure_condition=*/
      base::BindOnce(
          [](const base::FilePath& path) {
            // File size is 10, not a valid multiple of 32.
            base::WriteFile(path.AddExtensionASCII("32_foo"), "0123456789");
          },
          v4_store_path_),
      /*expected_result=*/
      ConvertExtensionBlocklistV4ToV5Result::kInvalidFileSize,
      /*expect_v4_hash_file_deleted=*/true);
}

TEST_F(V5StoreTest, TestExtensionMigrationFailureWriteV5) {
  base::FilePath v5_hash_file_path = store_path_.AddExtensionASCII("foo");
  RunExtensionMigrationFailureTest(
      /*v4_hash_file_size=*/32,
      /*setup_failure_condition=*/
      base::BindOnce(
          [](const base::FilePath& v4_path, const base::FilePath& v5_path) {
            // Add valid v4 data so that reading the v4 file works.
            std::string v4_data = "aapbdbdomjkkjkaonfhkkikfgjllcleb";
            base::WriteFile(v4_path.AddExtensionASCII("32_foo"), v4_data);
            // Make it fail to try to write to the v5 path later.
            ASSERT_TRUE(base::CreateDirectory(v5_path));
          },
          v4_store_path_, v5_hash_file_path),
      /*expected_result=*/
      ConvertExtensionBlocklistV4ToV5Result::kWriteV5Failed,
      /*expect_v4_hash_file_deleted=*/true,
      /*teardown_cleanup=*/
      base::BindOnce(
          [](const base::FilePath& path) { base::DeletePathRecursively(path); },
          v5_hash_file_path));
}

TEST_F(V5StoreTest, TestExtensionMigrationNoHashFiles) {
  WriteV4FileFormatProtoToFile(
      v4_store_path_, /*magic=*/0x600D71FE, /*file_version=*/9,
      /*client_state=*/"v4_version", /*checksum_sha256=*/"v4_checksum",
      /*hash_files=*/{});

  V5Store store(task_runner(), store_path_, /*prefix_size=*/16, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/true);
  EXPECT_EQ(V5StoreReadResult::kReadSuccess, ReadFromDisk(store));

  EXPECT_TRUE(base::PathExists(store_path_));
  EXPECT_FALSE(base::PathExists(v4_store_path_));
  EXPECT_EQ("v4_version", store.version());
  EXPECT_TRUE(GetHashPrefixList(store).view().empty());

  // Confirm no conversion metrics logged since no hash file was processed.
  histogram_tester_.ExpectTotalCount(
      "SafeBrowsing.V5Store.ConvertExtensionBlocklistV4ToV5Result", 0);
  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.V5Store.V4ToV5MigrationResult",
      V4ToV5MigrationResult::kV4ToV5MigrationSucceeded, 1);
}

TEST_F(V5StoreTest, TestExtensionMigrationFailureChecksumMismatch) {
  std::string dummy_checksum(32, 'x');
  RunExtensionMigrationChecksumTest(
      /*override_checksum=*/dummy_checksum,
      /*expect_success=*/false);
}

TEST_F(V5StoreTest, TestExtensionMigrationSuccessWithChecksum) {
  RunExtensionMigrationChecksumTest(
      /*override_checksum=*/std::nullopt,
      /*expect_success=*/true);
}

TEST_F(V5StoreTest, TestExtensionMigrationSuccessWithEmptyChecksum) {
  RunExtensionMigrationChecksumTest(
      /*override_checksum=*/"",
      /*expect_success=*/true);
}

TEST_F(V5StoreTest, TestVerifyChecksumInvalidEmptyChecksum) {
  V5StoreFileFormat file_format;
  file_format.set_magic_number(0x600D71FE);
  file_format.set_file_version(10);
  ListDetails* list_details = file_format.mutable_list_details();
  list_details->set_version("test_version");
  V5HashFile* hash_file = list_details->mutable_hash_file();
  hash_file->set_extension("foo");
  hash_file->set_file_size(4);

  base::WriteFile(store_path_, file_format.SerializeAsString());
  base::WriteFile(store_path_.AddExtensionASCII("foo"), "abcd");

  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  store.Initialize();
  EXPECT_TRUE(store.HasValidData());
  EXPECT_TRUE(GetExpectedChecksum(store).empty());

  // Fails because we have data but no checksum.
  EXPECT_FALSE(store.VerifyChecksum());
  ExpectChecksumHistograms(V5ApplyUpdateResult::kChecksumMismatchFailure);
}

TEST_F(V5StoreTest, TestVerifyChecksumInvalidEmptyStoreAndChecksum) {
  ListDetails list_details;
  list_details.set_version("test_version");
  WriteFileFormatProtoToFile(0x600D71FE, 10, &list_details);

  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  store.Initialize();
  EXPECT_TRUE(store.HasValidData());
  EXPECT_TRUE(GetExpectedChecksum(store).empty());
  EXPECT_TRUE(GetHashPrefixList(store).view().empty());

  // Fails because we have no checksum.
  EXPECT_FALSE(store.VerifyChecksum());
  ExpectChecksumHistograms(V5ApplyUpdateResult::kChecksumMismatchFailure);
}

TEST_F(V5StoreTest, TestVerifyChecksumValid) {
  V5StoreFileFormat file_format;
  file_format.set_magic_number(0x600D71FE);
  file_format.set_file_version(10);
  ListDetails* list_details = file_format.mutable_list_details();
  list_details->set_version("test_version");
  V5HashFile* hash_file = list_details->mutable_hash_file();
  hash_file->set_extension("foo");
  hash_file->set_file_size(4);

  std::string data = "abcd";
  std::array<uint8_t, crypto::hash::kSha256Size> checksum;
  crypto::hash::Hash(crypto::hash::HashKind::kSha256, base::as_byte_span(data),
                     checksum);
  std::string checksum_sha256(reinterpret_cast<char*>(checksum.data()),
                              checksum.size());
  list_details->mutable_checksum()->set_sha256(checksum_sha256);

  base::WriteFile(store_path_, file_format.SerializeAsString());
  base::WriteFile(store_path_.AddExtensionASCII("foo"), data);

  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  store.Initialize();
  EXPECT_TRUE(store.HasValidData());
  EXPECT_EQ(checksum_sha256, GetExpectedChecksum(store));

  EXPECT_TRUE(store.VerifyChecksum());
  EXPECT_EQ(checksum_sha256, GetExpectedChecksum(store));
  ExpectChecksumHistograms(V5ApplyUpdateResult::kSuccess);
}

TEST_F(V5StoreTest, TestVerifyChecksumInvalid) {
  V5StoreFileFormat file_format;
  file_format.set_magic_number(0x600D71FE);
  file_format.set_file_version(10);
  ListDetails* list_details = file_format.mutable_list_details();
  list_details->set_version("test_version");
  V5HashFile* hash_file = list_details->mutable_hash_file();
  hash_file->set_extension("foo");
  hash_file->set_file_size(4);

  std::string data = "abcd";
  std::string wrong_checksum(32, 'x');
  list_details->mutable_checksum()->set_sha256(wrong_checksum);

  base::WriteFile(store_path_, file_format.SerializeAsString());
  base::WriteFile(store_path_.AddExtensionASCII("foo"), data);

  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  store.Initialize();
  EXPECT_TRUE(store.HasValidData());
  EXPECT_EQ(wrong_checksum, GetExpectedChecksum(store));

  EXPECT_FALSE(store.VerifyChecksum());
  EXPECT_EQ(wrong_checksum, GetExpectedChecksum(store));
  ExpectChecksumHistograms(V5ApplyUpdateResult::kChecksumMismatchFailure);
}

TEST_F(V5StoreTest, TestVerifyChecksumEmptyStore) {
  ListDetails list_details;
  list_details.set_version("test_version");
  list_details.mutable_checksum()->set_sha256(std::string(32, 'x'));
  WriteFileFormatProtoToFile(0x600D71FE, 10, &list_details);

  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  store.Initialize();
  EXPECT_TRUE(store.HasValidData());
  EXPECT_EQ(std::string(32, 'x'), GetExpectedChecksum(store));
  EXPECT_TRUE(GetHashPrefixList(store).view().empty());

  EXPECT_FALSE(store.VerifyChecksum());
  ExpectChecksumHistograms(V5ApplyUpdateResult::kChecksumMismatchFailure);
}

TEST_F(V5StoreTest, TestVerifyChecksumSkippedOnReadFailure) {
  base::HistogramTester histogram_tester;
  // Write a corrupted file (invalid magic number).
  WriteFileFormatProtoToFile(111);

  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  store.Initialize();
  EXPECT_FALSE(store.HasValidData());

  // VerifyChecksum should return true and not log any histograms itself.
  EXPECT_TRUE(store.VerifyChecksum());
  histogram_tester.ExpectTotalCount(
      "SafeBrowsing.V5ReadFromDisk.ApplyUpdate.Result", 0);
  histogram_tester.ExpectTotalCount(
      "SafeBrowsing.V5ReadFromDisk.ApplyUpdate.Result.V5StoreTest", 0);
  histogram_tester.ExpectTotalCount(
      "SafeBrowsing.V5ReadFromDisk.VerifyChecksumDuration", 0);
  histogram_tester.ExpectTotalCount(
      "SafeBrowsing.SBReadFromDisk.VerifyChecksumDuration", 0);

  // The general read histogram should still be logged.
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5StoreRead.Result",
      V5StoreReadResult::kUnexpectedMagicNumberFailure, 1);
}

}  // namespace safe_browsing
