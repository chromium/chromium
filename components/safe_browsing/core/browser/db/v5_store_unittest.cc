// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v5_store.h"

#include <optional>

#include "base/containers/span.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/safe_browsing/core/browser/db/v4_store.pb.h"
#include "components/safe_browsing/core/browser/db/v5_rice.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"
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

  std::string SerializePrefixes(const std::vector<uint32_t>& prefixes) {
    std::string data;
    for (uint32_t val : prefixes) {
      data.append(base::as_string_view(base::U32ToBigEndian(val)));
    }
    return data;
  }

  int64_t GetFileSize(const V5Store& store) { return store.file_size_; }

  std::string GetExpectedChecksum(const V5Store& store) {
    return store.expected_checksum_;
  }

  void SetExpectedChecksum(V5Store& store, const std::string& checksum) {
    store.expected_checksum_ = checksum;
  }

  void VerifyStoreReadBack(const std::string& expected_version,
                           const std::string& expected_data,
                           const std::string& expected_checksum,
                           bool expect_hash_file,
                           std::optional<uint64_t> expected_hash_file_size) {
    V5Store read_store(task_runner(), store_path_, 4, v4_store_path_,
                       /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                       /*is_extensions_blocklist=*/false);
    read_store.Initialize();
    EXPECT_TRUE(read_store.HasValidData());
    EXPECT_EQ(expected_version, read_store.GetStoreState());
    if (expected_data.empty()) {
      EXPECT_TRUE(GetHashPrefixList(read_store).view().empty());
    } else {
      EXPECT_EQ(expected_data, GetHashPrefixList(read_store).view().at(4));
    }
    EXPECT_EQ(expected_checksum, GetExpectedChecksum(read_store));

    std::string proto_contents;
    ASSERT_TRUE(base::ReadFileToString(store_path_, &proto_contents));
    V5StoreFileFormat file_format;
    ASSERT_TRUE(file_format.ParseFromString(proto_contents));

    EXPECT_EQ(V5Store::kFileMagic, file_format.magic_number());
    EXPECT_EQ(V5Store::kV5FileVersion, file_format.file_version());

    if (expect_hash_file) {
      ASSERT_TRUE(file_format.list_details().has_hash_file());
      std::string extension =
          file_format.list_details().hash_file().extension();
      EXPECT_FALSE(extension.empty());
      EXPECT_TRUE(base::PathExists(
          HashPrefixContainer::GetPath(store_path_, extension)));

      if (expected_hash_file_size.has_value()) {
        EXPECT_EQ(expected_hash_file_size.value(),
                  file_format.list_details().hash_file().file_size());
        std::optional<int64_t> actual_size = base::GetFileSize(
            HashPrefixContainer::GetPath(store_path_, extension));
        ASSERT_TRUE(actual_size.has_value());
        EXPECT_EQ(expected_hash_file_size.value(),
                  static_cast<uint64_t>(actual_size.value()));
      }
    } else {
      EXPECT_FALSE(file_format.list_details().has_hash_file());
    }
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

  SBStorePtr RunApplyUpdateTest(V5Store& store,
                                std::unique_ptr<V5::HashList> hash_list) {
    base::RunLoop run_loop;
    SBStorePtr updated_store{nullptr, SBStoreDeleter(task_runner())};
    UpdatedStoreReadyCallback store_ready_callback = base::BindOnce(
        [](base::OnceClosure quit_closure, SBStorePtr* out_store,
           SBStorePtr store) {
          *out_store = std::move(store);
          std::move(quit_closure).Run();
        },
        run_loop.QuitClosure(), &updated_store);

    auto sb_response = std::make_unique<SBUpdateResponse>();
    sb_response->v5_response = std::move(hash_list);

    store.ApplyUpdate(std::move(sb_response), task_runner(),
                      std::move(store_ready_callback));
    run_loop.Run();
    return updated_store;
  }

  void CheckApplyUpdateHistograms(
      const std::string& partial_or_full,
      V5ApplyUpdateResult expected_apply_update_result,
      std::optional<V5DecodeResult> expected_decode_removals_result,
      std::optional<V5DecodeResult> expected_decode_additions_result,
      std::optional<size_t> expected_removals_count,
      std::optional<size_t> expected_additions_count,
      std::optional<V5StoreWriteResult> expected_write_result,
      const std::string& store_suffix) {
    // ApplyUpdate Result
    histogram_tester_.ExpectUniqueSample("SafeBrowsing.V5Process" +
                                             partial_or_full +
                                             "Update.ApplyUpdate.Result",
                                         expected_apply_update_result, 1);
    histogram_tester_.ExpectUniqueSample(
        "SafeBrowsing.V5Process" + partial_or_full +
            "Update.ApplyUpdate.Result" + store_suffix,
        expected_apply_update_result, 1);

    // ApplyUpdate Duration
    histogram_tester_.ExpectTotalCount("SafeBrowsing.V5Process" +
                                           partial_or_full +
                                           "Update.ApplyUpdateDuration",
                                       1);

    // Decode Removals Result
    if (expected_decode_removals_result.has_value()) {
      histogram_tester_.ExpectUniqueSample(
          "SafeBrowsing.V5Process" + partial_or_full +
              "Update.DecodeRemovals.Result",
          expected_decode_removals_result.value(), 1);
      histogram_tester_.ExpectUniqueSample(
          "SafeBrowsing.V5Process" + partial_or_full +
              "Update.DecodeRemovals.Result" + store_suffix,
          expected_decode_removals_result.value(), 1);
    } else {
      histogram_tester_.ExpectTotalCount("SafeBrowsing.V5Process" +
                                             partial_or_full +
                                             "Update.DecodeRemovals.Result",
                                         0);
      histogram_tester_.ExpectTotalCount(
          "SafeBrowsing.V5Process" + partial_or_full +
              "Update.DecodeRemovals.Result" + store_suffix,
          0);
    }

    // Removals Count
    if (expected_removals_count.has_value()) {
      histogram_tester_.ExpectUniqueSample("SafeBrowsing.V5Process" +
                                               partial_or_full +
                                               "Update.RemovalsHashesCount",
                                           expected_removals_count.value(), 1);
      histogram_tester_.ExpectUniqueSample(
          "SafeBrowsing.V5Process" + partial_or_full +
              "Update.RemovalsHashesCount" + store_suffix,
          expected_removals_count.value(), 1);
    } else {
      histogram_tester_.ExpectTotalCount("SafeBrowsing.V5Process" +
                                             partial_or_full +
                                             "Update.RemovalsHashesCount",
                                         0);
      histogram_tester_.ExpectTotalCount(
          "SafeBrowsing.V5Process" + partial_or_full +
              "Update.RemovalsHashesCount" + store_suffix,
          0);
    }

    // Decode Additions Result
    if (expected_decode_additions_result.has_value()) {
      histogram_tester_.ExpectUniqueSample(
          "SafeBrowsing.V5Process" + partial_or_full +
              "Update.DecodeAdditions.Result",
          expected_decode_additions_result.value(), 1);
      histogram_tester_.ExpectUniqueSample(
          "SafeBrowsing.V5Process" + partial_or_full +
              "Update.DecodeAdditions.Result" + store_suffix,
          expected_decode_additions_result.value(), 1);
    } else {
      histogram_tester_.ExpectTotalCount("SafeBrowsing.V5Process" +
                                             partial_or_full +
                                             "Update.DecodeAdditions.Result",
                                         0);
      histogram_tester_.ExpectTotalCount(
          "SafeBrowsing.V5Process" + partial_or_full +
              "Update.DecodeAdditions.Result" + store_suffix,
          0);
    }

    // Additions Count
    if (expected_additions_count.has_value()) {
      histogram_tester_.ExpectUniqueSample("SafeBrowsing.V5Process" +
                                               partial_or_full +
                                               "Update.AdditionsHashesCount",
                                           expected_additions_count.value(), 1);
      histogram_tester_.ExpectUniqueSample(
          "SafeBrowsing.V5Process" + partial_or_full +
              "Update.AdditionsHashesCount" + store_suffix,
          expected_additions_count.value(), 1);
    } else {
      histogram_tester_.ExpectTotalCount("SafeBrowsing.V5Process" +
                                             partial_or_full +
                                             "Update.AdditionsHashesCount",
                                         0);
      histogram_tester_.ExpectTotalCount(
          "SafeBrowsing.V5Process" + partial_or_full +
              "Update.AdditionsHashesCount" + store_suffix,
          0);
    }

    // Write Result
    if (expected_write_result.has_value()) {
      histogram_tester_.ExpectUniqueSample("SafeBrowsing.V5StoreWrite.Result",
                                           expected_write_result.value(), 1);
    } else {
      histogram_tester_.ExpectTotalCount("SafeBrowsing.V5StoreWrite.Result", 0);
    }
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

TEST_F(V5StoreTest, ApplyUpdateEmptySucceeds) {
  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);

  auto hash_list = std::make_unique<V5::HashList>();
  hash_list->set_version("new_test_version_123");
  auto hash = crypto::hash::Sha256(base::span<const uint8_t>());
  hash_list->set_sha256_checksum(std::string(hash.begin(), hash.end()));

  SBStorePtr updated_store = RunApplyUpdateTest(store, std::move(hash_list));

  ASSERT_TRUE(updated_store);

  EXPECT_TRUE(updated_store->HasValidData());
  EXPECT_EQ("new_test_version_123", updated_store->GetStoreState());

  V5Store* v5_updated_store = static_cast<V5Store*>(updated_store.get());
  EXPECT_TRUE(GetHashPrefixList(*v5_updated_store).view().empty());

  VerifyStoreReadBack(
      /*expected_version=*/"new_test_version_123",
      /*expected_data=*/"",
      /*expected_checksum=*/std::string(hash.begin(), hash.end()),
      /*expect_hash_file=*/false,
      /*expected_hash_file_size=*/std::nullopt);
  CheckApplyUpdateHistograms(
      "Full",
      /*expected_apply_update_result=*/V5ApplyUpdateResult::kSuccess,
      /*expected_decode_removals_result=*/V5DecodeResult::kSuccess,
      /*expected_decode_additions_result=*/V5DecodeResult::kSuccess,
      /*expected_removals_count=*/0,
      /*expected_additions_count=*/0,
      /*expected_write_result=*/V5StoreWriteResult::kWriteSuccess,
      /*store_suffix=*/".V5StoreTest");
}

TEST_F(V5StoreTest, ApplyUpdateMissingChecksumFailure) {
  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);

  auto hash_list = std::make_unique<V5::HashList>();
  hash_list->set_version("new_test_version_123");
  // No checksum on the update or on the original store.

  SBStorePtr updated_store = RunApplyUpdateTest(store, std::move(hash_list));

  EXPECT_FALSE(updated_store);

  CheckApplyUpdateHistograms(
      "Full",
      /*expected_apply_update_result=*/
      V5ApplyUpdateResult::kChecksumMismatchFailure,
      /*expected_decode_removals_result=*/V5DecodeResult::kSuccess,
      /*expected_decode_additions_result=*/V5DecodeResult::kSuccess,
      /*expected_removals_count=*/0,
      /*expected_additions_count=*/0,
      /*expected_write_result=*/std::nullopt,
      /*store_suffix=*/".V5StoreTest");
}

TEST_F(V5StoreTest, ApplyUpdateMissingChecksumSucceedsWithOldChecksum) {
  // From safebrowsingv5.proto `sha256_checksum` comments: "In the case that no
  // updates were provided, the server will omit this field to indicate that
  // the client should use the existing checksum."

  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  auto empty_hash = crypto::hash::Sha256(base::span<const uint8_t>());
  std::string empty_hash_str(empty_hash.begin(), empty_hash.end());
  SetExpectedChecksum(store, empty_hash_str);

  auto hash_list = std::make_unique<V5::HashList>();
  hash_list->set_version("new_test_version_123");

  SBStorePtr updated_store = RunApplyUpdateTest(store, std::move(hash_list));

  ASSERT_TRUE(updated_store);
  EXPECT_TRUE(updated_store->HasValidData());
  EXPECT_EQ("new_test_version_123", updated_store->GetStoreState());
  EXPECT_TRUE(updated_store->VerifyChecksum());

  CheckApplyUpdateHistograms(
      "Full",
      /*expected_apply_update_result=*/V5ApplyUpdateResult::kSuccess,
      /*expected_decode_removals_result=*/V5DecodeResult::kSuccess,
      /*expected_decode_additions_result=*/V5DecodeResult::kSuccess,
      /*expected_removals_count=*/0,
      /*expected_additions_count=*/0,
      /*expected_write_result=*/V5StoreWriteResult::kWriteSuccess,
      /*store_suffix=*/".V5StoreTest");
}

TEST_F(V5StoreTest, ApplyUpdateWithAdditions) {
  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);

  auto hash_list = std::make_unique<V5::HashList>();
  hash_list->set_version("new_version_with_entries");
  std::string expected_data = std::string("\x11\x22\x33\x44", 4);
  auto expected_checksum =
      crypto::hash::Sha256(base::as_byte_span(expected_data));
  std::string expected_checksum_str(expected_checksum.begin(),
                                    expected_checksum.end());
  hash_list->set_sha256_checksum(expected_checksum_str);

  // Single-entry additions:
  V5::RiceDeltaEncoded32Bit* additions =
      hash_list->mutable_additions_four_bytes();
  additions->set_entries_count(0);
  additions->set_first_value(0x11223344);
  additions->set_rice_parameter(3);

  SBStorePtr updated_store = RunApplyUpdateTest(store, std::move(hash_list));

  ASSERT_TRUE(updated_store);

  EXPECT_TRUE(updated_store->HasValidData());
  EXPECT_EQ("new_version_with_entries", updated_store->GetStoreState());

  V5Store* v5_updated_store = static_cast<V5Store*>(updated_store.get());
  EXPECT_EQ(expected_data, GetHashPrefixList(*v5_updated_store).view().at(4));
  VerifyStoreReadBack(
      /*expected_version=*/"new_version_with_entries",
      /*expected_data=*/expected_data,
      /*expected_checksum=*/expected_checksum_str,
      /*expect_hash_file=*/true,
      /*expected_hash_file_size=*/4);
  CheckApplyUpdateHistograms(
      "Full",
      /*expected_apply_update_result=*/V5ApplyUpdateResult::kSuccess,
      /*expected_decode_removals_result=*/V5DecodeResult::kSuccess,
      /*expected_decode_additions_result=*/V5DecodeResult::kSuccess,
      /*expected_removals_count=*/0,
      /*expected_additions_count=*/1,
      /*expected_write_result=*/V5StoreWriteResult::kWriteSuccess,
      /*store_suffix=*/".V5StoreTest");
}

TEST_F(V5StoreTest, ApplyUpdateFailsAdditions) {
  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);

  auto hash_list = std::make_unique<V5::HashList>();
  hash_list->set_version("failed_additions");

  // Additions with 1 entry but empty encoded data (should fail decode)
  V5::RiceDeltaEncoded32Bit* additions =
      hash_list->mutable_additions_four_bytes();
  additions->set_entries_count(1);
  additions->set_first_value(0x11223344);
  additions->set_rice_parameter(3);
  additions->set_encoded_data("");

  SBStorePtr updated_store = RunApplyUpdateTest(store, std::move(hash_list));

  EXPECT_FALSE(updated_store);

  CheckApplyUpdateHistograms(
      "Full",
      /*expected_apply_update_result=*/
      V5ApplyUpdateResult::kRiceDecodingAdditionsFailure,
      /*expected_decode_removals_result=*/V5DecodeResult::kSuccess,
      /*expected_decode_additions_result=*/
      V5DecodeResult::kRanOutOfBits,
      /*expected_removals_count=*/0,
      /*expected_additions_count=*/std::nullopt,
      /*expected_write_result=*/std::nullopt,
      /*store_suffix=*/".V5StoreTest");
}

TEST_F(V5StoreTest, ApplyUpdateFailsRemovals) {
  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);

  auto hash_list = std::make_unique<V5::HashList>();
  hash_list->set_version("failed_removals");

  // Removals with 1 entry but empty encoded data (should fail decode)
  V5::RiceDeltaEncoded32Bit* removals =
      hash_list->mutable_compressed_removals();
  removals->set_entries_count(1);
  removals->set_first_value(42);
  removals->set_rice_parameter(3);
  removals->set_encoded_data("");

  SBStorePtr updated_store = RunApplyUpdateTest(store, std::move(hash_list));

  EXPECT_FALSE(updated_store);

  CheckApplyUpdateHistograms("Full",
                             /*expected_apply_update_result=*/
                             V5ApplyUpdateResult::kRiceDecodingRemovalsFailure,
                             /*expected_decode_removals_result=*/
                             V5DecodeResult::kRanOutOfBits,
                             /*expected_decode_additions_result=*/std::nullopt,
                             /*expected_removals_count=*/std::nullopt,
                             /*expected_additions_count=*/std::nullopt,
                             /*expected_write_result=*/std::nullopt,
                             /*store_suffix=*/".V5StoreTest");
}

TEST_F(V5StoreTest, ApplyUpdateEmptyOnNonEmptyStoreSucceeds) {
  // 1. Write old store to disk with 1 item (4 bytes).
  ListDetails list_details;
  list_details.set_version("v5_old_version");
  list_details.mutable_hash_file()->set_extension("foo");
  list_details.mutable_hash_file()->set_file_size(4);

  std::string old_data = std::string("\x00\x00\x00\x0a", 4);
  std::array<uint8_t, crypto::hash::kSha256Size> old_checksum;
  crypto::hash::Hash(crypto::hash::HashKind::kSha256,
                     base::as_byte_span(old_data), old_checksum);
  list_details.mutable_checksum()->set_sha256(std::string(
      reinterpret_cast<char*>(old_checksum.data()), old_checksum.size()));

  WriteFileFormatProtoToFile(0x600D71FE, 10, &list_details);
  base::WriteFile(store_path_.AddExtensionASCII("foo"), old_data);

  // 2. Initialize store.
  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  store.Initialize();
  ASSERT_TRUE(store.HasValidData());

  // 3. Prepare update.
  auto hash_list = std::make_unique<V5::HashList>();
  hash_list->set_version("v5_new_version");
  hash_list->set_partial_update(true);
  // No checksum set in the update response.
  // The existing checksum from the old store should be preserved and written
  // to disk instead of being overwritten with a blank one.
  // No additions, no removals.

  // 4. Apply update.
  SBStorePtr updated_store = RunApplyUpdateTest(store, std::move(hash_list));

  // 5. Verify.
  ASSERT_TRUE(updated_store);

  EXPECT_TRUE(updated_store->HasValidData());
  EXPECT_EQ("v5_new_version", updated_store->GetStoreState());

  V5Store* v5_updated_store = static_cast<V5Store*>(updated_store.get());
  EXPECT_EQ(old_data, GetHashPrefixList(*v5_updated_store).view().at(4));
  std::string old_checksum_str(
      reinterpret_cast<const char*>(old_checksum.data()), old_checksum.size());
  // Verify that the checksum from the old store is preserved in the updated
  // store.
  EXPECT_EQ(old_checksum_str, GetExpectedChecksum(*v5_updated_store));

  VerifyStoreReadBack(
      /*expected_version=*/"v5_new_version",
      /*expected_data=*/old_data,
      /*expected_checksum=*/old_checksum_str,
      /*expect_hash_file=*/true,
      /*expected_hash_file_size=*/4);

  CheckApplyUpdateHistograms(
      "Partial",
      /*expected_apply_update_result=*/V5ApplyUpdateResult::kSuccess,
      /*expected_decode_removals_result=*/V5DecodeResult::kSuccess,
      /*expected_decode_additions_result=*/V5DecodeResult::kSuccess,
      /*expected_removals_count=*/0,
      /*expected_additions_count=*/0,
      /*expected_write_result=*/V5StoreWriteResult::kWriteSuccess,
      /*store_suffix=*/".V5StoreTest");
}

TEST_F(V5StoreTest, ApplyUpdateAdditionsDuplicateFailure) {
  // 1. Write old store to disk with 1 item: 10
  ListDetails list_details;
  list_details.set_version("v5_old_version");
  list_details.mutable_hash_file()->set_extension("foo");
  list_details.mutable_hash_file()->set_file_size(4);

  std::string old_data = SerializePrefixes({10});
  std::array<uint8_t, crypto::hash::kSha256Size> old_checksum;
  crypto::hash::Hash(crypto::hash::HashKind::kSha256,
                     base::as_byte_span(old_data), old_checksum);
  list_details.mutable_checksum()->set_sha256(std::string(
      reinterpret_cast<char*>(old_checksum.data()), old_checksum.size()));

  WriteFileFormatProtoToFile(0x600D71FE, 10, &list_details);
  base::WriteFile(store_path_.AddExtensionASCII("foo"), old_data);

  // 2. Initialize store
  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  store.Initialize();
  ASSERT_TRUE(store.HasValidData());

  // 3. Prepare update: Add 10 (duplicate)
  auto hash_list = std::make_unique<V5::HashList>();
  hash_list->set_version("v5_new_version");
  hash_list->set_partial_update(true);

  auto* additions_proto = hash_list->mutable_additions_four_bytes();
  additions_proto->set_rice_parameter(3);
  additions_proto->set_first_value(10);
  additions_proto->set_entries_count(0);

  // 4. Apply (expect failure)
  SBStorePtr updated_store = RunApplyUpdateTest(store, std::move(hash_list));
  EXPECT_FALSE(updated_store);

  // 5. Verify Histograms
  CheckApplyUpdateHistograms(
      "Partial",
      /*expected_apply_update_result=*/
      V5ApplyUpdateResult::kAdditionsHasExistingPrefixFailure,
      /*expected_decode_removals_result=*/V5DecodeResult::kSuccess,
      /*expected_decode_additions_result=*/V5DecodeResult::kSuccess,
      /*expected_removals_count=*/0,
      /*expected_additions_count=*/1,
      /*expected_write_result=*/std::nullopt,
      /*store_suffix=*/".V5StoreTest");
}

TEST_F(V5StoreTest, ApplyUpdateRemovalsIndexTooLargeFailure) {
  // 1. Write old store to disk with 1 item: 10
  ListDetails list_details;
  list_details.set_version("v5_old_version");
  list_details.mutable_hash_file()->set_extension("foo");
  list_details.mutable_hash_file()->set_file_size(4);

  std::string old_data = SerializePrefixes({10});
  std::array<uint8_t, crypto::hash::kSha256Size> old_checksum;
  crypto::hash::Hash(crypto::hash::HashKind::kSha256,
                     base::as_byte_span(old_data), old_checksum);
  list_details.mutable_checksum()->set_sha256(std::string(
      reinterpret_cast<char*>(old_checksum.data()), old_checksum.size()));

  WriteFileFormatProtoToFile(0x600D71FE, 10, &list_details);
  base::WriteFile(store_path_.AddExtensionASCII("foo"), old_data);

  // 2. Initialize store
  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  store.Initialize();
  ASSERT_TRUE(store.HasValidData());

  // 3. Prepare update: Remove index 1 (out of bounds, size is 1)
  auto hash_list = std::make_unique<V5::HashList>();
  hash_list->set_version("v5_new_version");
  hash_list->set_partial_update(true);

  auto* removals_proto = hash_list->mutable_compressed_removals();
  removals_proto->set_rice_parameter(3);
  removals_proto->set_first_value(1);
  removals_proto->set_entries_count(0);

  // 4. Apply (expect failure)
  SBStorePtr updated_store = RunApplyUpdateTest(store, std::move(hash_list));
  EXPECT_FALSE(updated_store);

  // 5. Verify Histograms
  CheckApplyUpdateHistograms(
      "Partial",
      /*expected_apply_update_result=*/
      V5ApplyUpdateResult::kRemovalsIndexTooLargeFailure,
      /*expected_decode_removals_result=*/V5DecodeResult::kSuccess,
      /*expected_decode_additions_result=*/V5DecodeResult::kSuccess,
      /*expected_removals_count=*/1,
      /*expected_additions_count=*/0,
      /*expected_write_result=*/std::nullopt,
      /*store_suffix=*/".V5StoreTest");
}

TEST_F(V5StoreTest, ApplyUpdateRemovalsIndexTooLargeFailureEmptyStore) {
  // 1. Write old store to disk: empty but versioned
  ListDetails list_details;
  list_details.set_version("v5_old_version");
  WriteFileFormatProtoToFile(0x600D71FE, 10, &list_details);

  // 2. Initialize store
  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  store.Initialize();
  ASSERT_TRUE(store.HasValidData());

  // 3. Prepare update: Remove index 0 (out of bounds, size is 0) and add 10.
  auto hash_list = std::make_unique<V5::HashList>();
  hash_list->set_version("v5_new_version");
  hash_list->set_partial_update(true);
  hash_list->set_sha256_checksum("dummy_checksum_32_bytes_long____");

  auto* removals_proto = hash_list->mutable_compressed_removals();
  removals_proto->set_rice_parameter(3);
  removals_proto->set_first_value(0);
  removals_proto->set_entries_count(0);

  auto* additions_proto = hash_list->mutable_additions_four_bytes();
  additions_proto->set_rice_parameter(3);
  additions_proto->set_first_value(10);
  additions_proto->set_entries_count(0);

  // 4. Apply (expect failure)
  SBStorePtr updated_store = RunApplyUpdateTest(store, std::move(hash_list));
  EXPECT_FALSE(updated_store);

  // 5. Verify Histograms
  CheckApplyUpdateHistograms(
      "Partial",
      /*expected_apply_update_result=*/
      V5ApplyUpdateResult::kRemovalsIndexTooLargeFailure,
      /*expected_decode_removals_result=*/V5DecodeResult::kSuccess,
      /*expected_decode_additions_result=*/V5DecodeResult::kSuccess,
      /*expected_removals_count=*/1,
      /*expected_additions_count=*/1,
      /*expected_write_result=*/std::nullopt,
      /*store_suffix=*/".V5StoreTest");
}

TEST_F(V5StoreTest, ApplyUpdateChecksumMismatchFailure) {
  // 1. Write old store to disk with 1 item: 10
  ListDetails list_details;
  list_details.set_version("v5_old_version");
  list_details.mutable_hash_file()->set_extension("foo");
  list_details.mutable_hash_file()->set_file_size(4);

  std::string old_data = SerializePrefixes({10});
  std::array<uint8_t, crypto::hash::kSha256Size> old_checksum;
  crypto::hash::Hash(crypto::hash::HashKind::kSha256,
                     base::as_byte_span(old_data), old_checksum);
  list_details.mutable_checksum()->set_sha256(std::string(
      reinterpret_cast<char*>(old_checksum.data()), old_checksum.size()));

  WriteFileFormatProtoToFile(0x600D71FE, 10, &list_details);
  base::WriteFile(store_path_.AddExtensionASCII("foo"), old_data);

  // 2. Initialize store
  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  store.Initialize();
  ASSERT_TRUE(store.HasValidData());

  // 3. Prepare update: Add 20
  auto hash_list = std::make_unique<V5::HashList>();
  hash_list->set_version("v5_new_version");
  hash_list->set_partial_update(true);

  auto* additions_proto = hash_list->mutable_additions_four_bytes();
  additions_proto->set_rice_parameter(3);
  additions_proto->set_first_value(20);
  additions_proto->set_entries_count(0);

  // Set invalid checksum (32 'a's)
  hash_list->set_sha256_checksum(std::string(32, 'a'));

  // 4. Apply (expect failure due to checksum mismatch)
  SBStorePtr updated_store = RunApplyUpdateTest(store, std::move(hash_list));
  EXPECT_FALSE(updated_store);

  // 5. Verify Histograms
  CheckApplyUpdateHistograms(
      "Partial", V5ApplyUpdateResult::kChecksumMismatchFailure,
      /*expected_decode_removals_result=*/V5DecodeResult::kSuccess,
      /*expected_decode_additions_result=*/V5DecodeResult::kSuccess,
      /*expected_removals_count=*/0,
      /*expected_additions_count=*/1,
      /*expected_write_result=*/std::nullopt,
      /*store_suffix=*/".V5StoreTest");
}

TEST_F(V5StoreTest, ApplyUpdateWithAdditionsAndEmptyChecksumFailure) {
  // 1. Write old store to disk with 1 item: 10
  ListDetails list_details;
  list_details.set_version("v5_old_version");
  list_details.mutable_hash_file()->set_extension("foo");
  list_details.mutable_hash_file()->set_file_size(4);

  std::string old_data = SerializePrefixes({10});
  std::array<uint8_t, crypto::hash::kSha256Size> old_checksum;
  crypto::hash::Hash(crypto::hash::HashKind::kSha256,
                     base::as_byte_span(old_data), old_checksum);
  list_details.mutable_checksum()->set_sha256(std::string(
      reinterpret_cast<char*>(old_checksum.data()), old_checksum.size()));

  WriteFileFormatProtoToFile(0x600D71FE, 10, &list_details);
  base::WriteFile(store_path_.AddExtensionASCII("foo"), old_data);

  // 2. Initialize store
  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  store.Initialize();
  ASSERT_TRUE(store.HasValidData());

  // 3. Prepare update: Add 20, but no checksum in response
  auto hash_list = std::make_unique<V5::HashList>();
  hash_list->set_version("v5_new_version");
  hash_list->set_partial_update(true);

  auto* additions_proto = hash_list->mutable_additions_four_bytes();
  additions_proto->set_rice_parameter(3);
  additions_proto->set_first_value(20);
  additions_proto->set_entries_count(0);

  // No checksum set.

  // 4. Apply (expect failure due to checksum mismatch because it should fall
  // back to the old checksum which doesn't match the new data)
  SBStorePtr updated_store = RunApplyUpdateTest(store, std::move(hash_list));
  EXPECT_FALSE(updated_store);

  // 5. Verify Histograms
  CheckApplyUpdateHistograms(
      "Partial", V5ApplyUpdateResult::kChecksumMismatchFailure,
      /*expected_decode_removals_result=*/V5DecodeResult::kSuccess,
      /*expected_decode_additions_result=*/V5DecodeResult::kSuccess,
      /*expected_removals_count=*/0,
      /*expected_additions_count=*/1,
      /*expected_write_result=*/std::nullopt,
      /*store_suffix=*/".V5StoreTest");
}

TEST_F(V5StoreTest, ApplyUpdateFullUpdateEmptiesStore) {
  // 1. Write old store to disk with 2 items: 10, 20
  ListDetails list_details;
  list_details.set_version("v5_old_version");
  list_details.mutable_hash_file()->set_extension("foo");
  list_details.mutable_hash_file()->set_file_size(8);

  std::string old_data = SerializePrefixes({10, 20});
  std::array<uint8_t, crypto::hash::kSha256Size> old_checksum;
  crypto::hash::Hash(crypto::hash::HashKind::kSha256,
                     base::as_byte_span(old_data), old_checksum);
  list_details.mutable_checksum()->set_sha256(std::string(
      reinterpret_cast<char*>(old_checksum.data()), old_checksum.size()));

  WriteFileFormatProtoToFile(0x600D71FE, 10, &list_details);
  base::WriteFile(store_path_.AddExtensionASCII("foo"), old_data);

  // 2. Initialize store
  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  store.Initialize();
  ASSERT_TRUE(store.HasValidData());

  // 3. Prepare full update with no additions/removals (clears store)
  auto hash_list = std::make_unique<V5::HashList>();
  hash_list->set_version("v5_new_version");
  hash_list->set_partial_update(false);  // Full update

  // Checksum of empty store (SHA256 of empty string)
  std::array<uint8_t, crypto::hash::kSha256Size> empty_checksum;
  crypto::hash::Hash(crypto::hash::HashKind::kSha256,
                     base::span<const uint8_t>(), empty_checksum);
  hash_list->set_sha256_checksum(std::string(
      reinterpret_cast<char*>(empty_checksum.data()), empty_checksum.size()));

  // 4. Apply
  SBStorePtr updated_store = RunApplyUpdateTest(store, std::move(hash_list));

  // 5. Verify (Success, empty store)
  ASSERT_TRUE(updated_store);

  EXPECT_TRUE(updated_store->HasValidData());
  EXPECT_EQ("v5_new_version", updated_store->GetStoreState());

  // Verify we can read it back.
  V5Store read_store(task_runner(), store_path_, 4, v4_store_path_,
                     /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                     /*is_extensions_blocklist=*/false);
  read_store.Initialize();
  EXPECT_TRUE(read_store.HasValidData());
  EXPECT_EQ("v5_new_version", read_store.GetStoreState());
  EXPECT_TRUE(GetHashPrefixList(read_store).view().empty());

  // Verify Histograms
  CheckApplyUpdateHistograms(
      "Full", V5ApplyUpdateResult::kSuccess,
      /*expected_decode_removals_result=*/V5DecodeResult::kSuccess,
      /*expected_decode_additions_result=*/V5DecodeResult::kSuccess,
      /*expected_removals_count=*/0,
      /*expected_additions_count=*/0,
      /*expected_write_result=*/V5StoreWriteResult::kWriteSuccess,
      /*store_suffix=*/".V5StoreTest");
}

TEST_F(V5StoreTest, ApplyUpdateWithAdditionsAndRemovals) {
  // 1. Write old store to disk with 6 items: 10, 20, 30, 40, 50, 60
  ListDetails list_details;
  list_details.set_version("v5_old_version");
  list_details.mutable_hash_file()->set_extension("foo");
  list_details.mutable_hash_file()->set_file_size(24);

  std::string old_data = SerializePrefixes({10, 20, 30, 40, 50, 60});
  std::array<uint8_t, crypto::hash::kSha256Size> old_checksum;
  crypto::hash::Hash(crypto::hash::HashKind::kSha256,
                     base::as_byte_span(old_data), old_checksum);
  list_details.mutable_checksum()->set_sha256(std::string(
      reinterpret_cast<char*>(old_checksum.data()), old_checksum.size()));

  WriteFileFormatProtoToFile(0x600D71FE, 10, &list_details);
  base::WriteFile(store_path_.AddExtensionASCII("foo"), old_data);

  // 2. Initialize store
  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);
  store.Initialize();
  ASSERT_TRUE(store.HasValidData());

  // 3. Prepare update:
  // Removals: index 1 (20), 2 (30), 4 (50), 5 (60).
  // Rice encoded removals: first_value = 1, deltas = [1, 2, 1] (indices: 1, 2,
  // 4, 5) Additions: [15, 25, 26, 45, 70, 71]
  auto hash_list = std::make_unique<V5::HashList>();
  hash_list->set_version("v5_new_version");
  hash_list->set_partial_update(true);

  auto* removals_proto = hash_list->mutable_compressed_removals();
  removals_proto->set_rice_parameter(3);
  removals_proto->set_first_value(1);
  removals_proto->set_entries_count(3);
  removals_proto->set_encoded_data("\x42\x02");

  auto* additions_proto = hash_list->mutable_additions_four_bytes();
  additions_proto->set_rice_parameter(3);
  additions_proto->set_first_value(15);
  additions_proto->set_entries_count(5);
  additions_proto->set_encoded_data(std::string("\x49\xB6\x8B\x00", 4));

  // Checksum of expected result [10, 15, 25, 26, 40, 45, 70, 71]
  std::string expected_data =
      SerializePrefixes({10, 15, 25, 26, 40, 45, 70, 71});
  std::array<uint8_t, crypto::hash::kSha256Size> checksum;
  crypto::hash::Hash(crypto::hash::HashKind::kSha256,
                     base::as_byte_span(expected_data), checksum);
  std::string expected_checksum_str(reinterpret_cast<char*>(checksum.data()),
                                    checksum.size());
  hash_list->set_sha256_checksum(expected_checksum_str);

  // 4. Apply
  SBStorePtr updated_store = RunApplyUpdateTest(store, std::move(hash_list));

  // 5. Verify
  ASSERT_TRUE(updated_store);

  EXPECT_TRUE(updated_store->HasValidData());
  EXPECT_EQ("v5_new_version", updated_store->GetStoreState());

  V5Store* v5_updated_store = static_cast<V5Store*>(updated_store.get());
  EXPECT_EQ(expected_data, GetHashPrefixList(*v5_updated_store).view().at(4));
  VerifyStoreReadBack(
      /*expected_version=*/"v5_new_version",
      /*expected_data=*/expected_data,
      /*expected_checksum=*/expected_checksum_str,
      /*expect_hash_file=*/true,
      /*expected_hash_file_size=*/32);

  // Verify Histograms
  CheckApplyUpdateHistograms(
      "Partial", V5ApplyUpdateResult::kSuccess,
      /*expected_decode_removals_result=*/V5DecodeResult::kSuccess,
      /*expected_decode_additions_result=*/V5DecodeResult::kSuccess,
      /*expected_removals_count=*/4,
      /*expected_additions_count=*/6,
      /*expected_write_result=*/V5StoreWriteResult::kWriteSuccess,
      /*store_suffix=*/".V5StoreTest");
}

TEST_F(V5StoreTest, WriteToDiskFails_Rename) {
  base::FilePath directory_store_path =
      temp_dir_.GetPath().AppendASCII("failure.store");
  ASSERT_TRUE(base::CreateDirectory(directory_store_path));
  V5Store store(task_runner(), directory_store_path, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);

  auto hash_list = std::make_unique<V5::HashList>();
  hash_list->set_version("new_version");
  auto empty_hash = crypto::hash::Sha256(base::span<const uint8_t>());
  hash_list->set_sha256_checksum(
      std::string(empty_hash.begin(), empty_hash.end()));

  SBStorePtr updated_store = RunApplyUpdateTest(store, std::move(hash_list));
  EXPECT_FALSE(updated_store);

  CheckApplyUpdateHistograms(
      "Full", V5ApplyUpdateResult::kWriteFailure,
      /*expected_decode_removals_result=*/V5DecodeResult::kSuccess,
      /*expected_decode_additions_result=*/V5DecodeResult::kSuccess,
      /*expected_removals_count=*/0,
      /*expected_additions_count=*/0,
      /*expected_write_result=*/V5StoreWriteResult::kUnableToRenameFailure,
      /*store_suffix=*/".failure");
}

TEST_F(V5StoreTest, WriteToDiskFails_Write) {
  base::FilePath non_writable_dir =
      temp_dir_.GetPath()
          .Append(FILE_PATH_LITERAL("nonexistent_dir"))
          .Append(FILE_PATH_LITERAL("some.store"));
  V5Store store(task_runner(), non_writable_dir, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);

  auto hash_list = std::make_unique<V5::HashList>();
  hash_list->set_version("new_version");
  auto empty_hash = crypto::hash::Sha256(base::span<const uint8_t>());
  hash_list->set_sha256_checksum(
      std::string(empty_hash.begin(), empty_hash.end()));

  SBStorePtr updated_store = RunApplyUpdateTest(store, std::move(hash_list));
  EXPECT_FALSE(updated_store);

  CheckApplyUpdateHistograms(
      "Full", V5ApplyUpdateResult::kWriteFailure,
      /*expected_decode_removals_result=*/V5DecodeResult::kSuccess,
      /*expected_decode_additions_result=*/V5DecodeResult::kSuccess,
      /*expected_removals_count=*/0,
      /*expected_additions_count=*/0,
      /*expected_write_result=*/
      V5StoreWriteResult::kUnexpectedBytesWrittenFailure,
      /*store_suffix=*/".some");
}

TEST_F(V5StoreTest, CleanUpOldFiles) {
  // Create dummy files.
  base::FilePath dummy_file1 = store_path_.AddExtensionASCII("dummy1");
  base::FilePath dummy_file2 = store_path_.AddExtensionASCII("dummy2");
  ASSERT_TRUE(base::WriteFile(dummy_file1, "stuff"));
  ASSERT_TRUE(base::WriteFile(dummy_file2, "other_stuff"));

  V5Store store(task_runner(), store_path_, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);

  // Apply update with additions to create a new hash file.
  auto hash_list = std::make_unique<V5::HashList>();
  hash_list->set_version("new_version");
  std::string expected_data = std::string("\x11\x22\x33\x44", 4);
  std::array<uint8_t, crypto::hash::kSha256Size> expected_checksum;
  crypto::hash::Hash(crypto::hash::HashKind::kSha256,
                     base::as_byte_span(expected_data), expected_checksum);
  hash_list->set_sha256_checksum(
      std::string(reinterpret_cast<char*>(expected_checksum.data()),
                  expected_checksum.size()));

  V5::RiceDeltaEncoded32Bit* additions =
      hash_list->mutable_additions_four_bytes();
  additions->set_entries_count(0);
  additions->set_first_value(0x11223344);
  additions->set_rice_parameter(3);

  SBStorePtr updated_store = RunApplyUpdateTest(store, std::move(hash_list));
  ASSERT_TRUE(updated_store);

  // The dummy files should be deleted.
  EXPECT_FALSE(base::PathExists(dummy_file1));
  EXPECT_FALSE(base::PathExists(dummy_file2));

  // The main store file should exist.
  EXPECT_TRUE(base::PathExists(store_path_));

  // The new hash file should exist.
  std::string proto_contents;
  ASSERT_TRUE(base::ReadFileToString(store_path_, &proto_contents));
  V5StoreFileFormat file_format;
  ASSERT_TRUE(file_format.ParseFromString(proto_contents));
  ASSERT_TRUE(file_format.list_details().has_hash_file());
  base::FilePath hash_file_path = HashPrefixContainer::GetPath(
      store_path_, file_format.list_details().hash_file().extension());
  EXPECT_TRUE(base::PathExists(hash_file_path));
}

// TODO(crbug.com/362791941): Enable this test once the write to disk file leak
// bug is fixed.
TEST_F(V5StoreTest, DISABLED_WriteToDiskFails_DeleteHashFile) {
  base::FilePath directory_store_path =
      temp_dir_.GetPath().AppendASCII("failure_additions.store");
  ASSERT_TRUE(base::CreateDirectory(directory_store_path));
  V5Store store(task_runner(), directory_store_path, 4, v4_store_path_,
                /*is_eligible_for_v4_to_v5_disk_migration=*/true,
                /*is_extensions_blocklist=*/false);

  auto hash_list = std::make_unique<V5::HashList>();
  hash_list->set_version("new_version");
  V5::RiceDeltaEncoded32Bit* additions =
      hash_list->mutable_additions_four_bytes();
  additions->set_entries_count(0);
  additions->set_first_value(0x11223344);
  additions->set_rice_parameter(3);

  SBStorePtr updated_store = RunApplyUpdateTest(store, std::move(hash_list));
  EXPECT_FALSE(updated_store);

  // Verify that no hash files were left over in the temp directory.
  // The directory itself should still exist, but there should be no files.
  base::FileEnumerator enumerator(temp_dir_.GetPath(), /*recursive=*/false,
                                  base::FileEnumerator::FILES);
  EXPECT_TRUE(enumerator.Next().empty());
}

}  // namespace safe_browsing
