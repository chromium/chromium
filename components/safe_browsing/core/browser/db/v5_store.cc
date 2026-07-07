// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v5_store.h"

#include <optional>
#include <set>
#include <utility>

#include "base/containers/span.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected.h"
#include "components/crx_file/id_util.h"
#include "components/safe_browsing/core/browser/db/hash_prefix_container.h"
#include "components/safe_browsing/core/browser/db/sb_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/v4_store.pb.h"
#include "components/safe_browsing/core/browser/db/v5_hash_list_rice_decoder.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"
#include "components/safe_browsing/core/common/proto/v5_store.pb.h"
#include "crypto/hash.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace safe_browsing {

namespace {

const char kApplyUpdate[] = ".ApplyUpdate";
const char kResult[] = ".Result";

void RecordStoreReadResult(V5StoreReadResult result) {
  base::UmaHistogramEnumeration("SafeBrowsing.V5StoreRead.Result", result);
  base::UmaHistogramBoolean("SafeBrowsing.SBStoreRead.Success",
                            result == V5StoreReadResult::kReadSuccess);
}

template <typename T>
void RecordEnumWithAndWithoutSuffix(const std::string& metric,
                                    T value,
                                    const base::FilePath& file_path) {
  base::UmaHistogramEnumeration(metric + kResult, value);
  std::string suffix = GetUmaSuffixForStore(file_path);
  base::UmaHistogramEnumeration(metric + kResult + suffix, value);
}

void RecordApplyUpdateResult(const std::string& base_metric,
                             V5ApplyUpdateResult result,
                             const base::FilePath& file_path) {
  RecordEnumWithAndWithoutSuffix(base_metric + kApplyUpdate, result, file_path);
}

void RecordApplyUpdateDuration(const std::string& base_metric,
                               base::TimeDelta elapsed) {
  base::UmaHistogramTimes(base_metric + ".ApplyUpdateDuration", elapsed);
}

void RecordDecodeRemovalsResult(const std::string& base_metric,
                                V5DecodeResult result,
                                const base::FilePath& file_path) {
  RecordEnumWithAndWithoutSuffix(base_metric + ".DecodeRemovals", result,
                                 file_path);
}

void RecordRemovalsHashesCount(const std::string& base_metric,
                               size_t count,
                               const base::FilePath& file_path) {
  std::string metric = base_metric + ".RemovalsHashesCount";
  base::UmaHistogramCounts10000(metric, count);
  std::string suffix = GetUmaSuffixForStore(file_path);
  base::UmaHistogramCounts10000(metric + suffix, count);
}

void RecordDecodeAdditionsResult(const std::string& base_metric,
                                 V5DecodeResult result,
                                 const base::FilePath& file_path) {
  RecordEnumWithAndWithoutSuffix(base_metric + ".DecodeAdditions", result,
                                 file_path);
}

void RecordAdditionsHashesCount(const std::string& base_metric,
                                bool is_full_update,
                                size_t count,
                                const base::FilePath& file_path) {
  std::string metric = base_metric + ".AdditionsHashesCount";
  std::string suffix = GetUmaSuffixForStore(file_path);
  int exclusive_max = is_full_update ? 5000000 : 100000;
  base::UmaHistogramCustomCounts(metric, count, /*min=*/1, exclusive_max,
                                 /*buckets=*/50);
  base::UmaHistogramCustomCounts(metric + suffix, count, /*min=*/1,
                                 exclusive_max, /*buckets=*/50);
}

void RecordMigrationTime(base::TimeDelta elapsed,
                         const base::FilePath& store_path) {
  std::string suffix = GetUmaSuffixForStore(store_path);
  base::UmaHistogramTimes(
      "SafeBrowsing.V5Store.V4ToV5Migration.TimeTaken" + suffix, elapsed);
}

void RecordStoreWriteResult(V5StoreWriteResult result) {
  base::UmaHistogramEnumeration("SafeBrowsing.V5StoreWrite.Result", result);
}

// Cleans up files that are no longer needed after a successful write.
// TODO(crbug.com/362791941): This implementation is copied over from the v4
// implementation, but it has a bug where if there are mmap-ed files, they are
// not able to be deleted on Windows until the subsequent update. This cleanup
// should be moved to after the mmap-ed files are released.
void CleanupExtraFiles(const base::FilePath& store_path,
                       const V5StoreFileFormat& file_format) {
  std::set<base::FilePath> paths_in_use{store_path};
  if (file_format.list_details().has_hash_file()) {
    paths_in_use.insert(HashPrefixContainer::GetPath(
        store_path, file_format.list_details().hash_file().extension()));
  }

  // Iterate through all files that start with the store path name. All hash
  // files will be the store path plus an extension.
  base::FileEnumerator e(
      store_path.DirName(), false, base::FileEnumerator::FILES,
      store_path.BaseName().value() + FILE_PATH_LITERAL(".*"));
  for (base::FilePath name = e.Next(); !name.empty(); name = e.Next()) {
    if (paths_in_use.find(name) == paths_in_use.end()) {
      base::DeleteFile(name);
    }
  }
}

}  // namespace

V5Store::V5Store(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                 const base::FilePath& store_path,
                 PrefixSize prefix_size,
                 const base::FilePath& v4_store_path,
                 bool is_eligible_for_v4_to_v5_disk_migration,
                 bool is_extensions_blocklist,
                 const int64_t old_file_size)
    : SBStore(task_runner, store_path, old_file_size),
      hash_prefix_list_(std::make_unique<HashPrefixList>(store_path,
                                                         prefix_size,
                                                         task_runner)),
      prefix_size_(prefix_size),
      v4_store_path_(v4_store_path),
      is_eligible_for_migration_(is_eligible_for_v4_to_v5_disk_migration),
      is_extensions_blocklist_(is_extensions_blocklist) {}

V5Store::~V5Store() = default;

void V5Store::Initialize() {
  CHECK(version_.empty());

  V5StoreReadResult store_read_result = ReadFromDisk();
  has_valid_data_ = (store_read_result == V5StoreReadResult::kReadSuccess);
  RecordStoreReadResult(store_read_result);
}

std::string V5Store::GetMetricPrefix() const {
  return "SafeBrowsing.V5Store";
}

V5StoreReadResult V5Store::ReadFromDisk() {
  CHECK(task_runner_->RunsTasksInCurrentSequence());

  V4ToV5MigrationResult migration_result = AttemptV4ToV5Migration();
  base::UmaHistogramEnumeration("SafeBrowsing.V5Store.V4ToV5MigrationResult",
                                migration_result);

  switch (migration_result) {
    case V4ToV5MigrationResult::kDiskAlreadyV5:
    case V4ToV5MigrationResult::kV4ToV5MigrationSucceeded:
      return ReadFromDiskInternal();
    case V4ToV5MigrationResult::kStoreIneligibleWipeSucceeded:
      return V5StoreReadResult::kV4ToV5MigrationWipedSuccessfully;
    case V4ToV5MigrationResult::kV4StoreNotFound:
      return V5StoreReadResult::kFileOpenFailure;
    case V4ToV5MigrationResult::kReadV4Failed:
    case V4ToV5MigrationResult::kMultipleHashFilesFailure:
    case V4ToV5MigrationResult::kPrefixSizeMismatchFailure:
    case V4ToV5MigrationResult::kHashFileMissingFailure:
    case V4ToV5MigrationResult::kRenameHashFileFailure:
    case V4ToV5MigrationResult::kWriteV5FileFailure:
    case V4ToV5MigrationResult::kProtoSerializationFailure:
    case V4ToV5MigrationResult::kExtensionParsingFailure:
    case V4ToV5MigrationResult::kRenameV5StoreFileFailure:
    case V4ToV5MigrationResult::kStoreIneligibleWipeFailed:
    case V4ToV5MigrationResult::kExtensionBlocklistMigrationFailed:
      return V5StoreReadResult::kV4ToV5MigrationFailure;
  }
}

V4ToV5MigrationResult V5Store::AttemptV4ToV5Migration() {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  CHECK(!v4_store_path_.empty());

  if (base::PathExists(store_path_)) {
    return V4ToV5MigrationResult::kDiskAlreadyV5;
  }
  if (!base::PathExists(v4_store_path_)) {
    return V4ToV5MigrationResult::kV4StoreNotFound;
  }

  base::ElapsedTimer timer;
  absl::Cleanup log_timer = [this, &timer] {
    RecordMigrationTime(timer.Elapsed(), store_path_);
  };

  if (!is_eligible_for_migration_) {
    bool wipe_succeeded = WipeV4Store(v4_store_path_);
    return wipe_succeeded ? V4ToV5MigrationResult::kStoreIneligibleWipeSucceeded
                          : V4ToV5MigrationResult::kStoreIneligibleWipeFailed;
  }
  return MigrateFromV4(v4_store_path_);
}

V5StoreReadResult V5Store::ReadFromDiskInternal() {
  CHECK(task_runner_->RunsTasksInCurrentSequence());

  V5StoreFileFormat file_format;
  int64_t file_size;
  V5StoreReadResult validation_result =
      ParseAndValidateV5StoreFileFormat(store_path_, file_format, &file_size);
  if (validation_result != V5StoreReadResult::kReadSuccess) {
    return validation_result;
  }

  V5ApplyUpdateResult apply_update_result =
      hash_prefix_list_->ReadFromDisk(SBStoreFileFormat(&file_format));
  last_apply_update_result_ = apply_update_result;

  if (apply_update_result != V5ApplyUpdateResult::kSuccess) {
    hash_prefix_list_->Clear();
    // The success case will be logged within `VerifyChecksum`.
    RecordApplyUpdateResult("SafeBrowsing.V5ReadFromDisk", apply_update_result,
                            store_path_);
    return V5StoreReadResult::kHashPrefixListGenerationFailure;
  }

  if (file_format.list_details().has_version()) {
    version_ = file_format.list_details().version();
  }
  if (file_format.list_details().has_checksum() &&
      file_format.list_details().checksum().has_sha256()) {
    expected_checksum_ = file_format.list_details().checksum().sha256();
  }

  // Update |file_size_| now because we parsed the file correctly.
  file_size_ = file_size;
  if (file_format.list_details().has_hash_file()) {
    file_size_ += file_format.list_details().hash_file().file_size();
  }

  return V5StoreReadResult::kReadSuccess;
}

V4ToV5MigrationResult V5Store::MigrateFromV4(
    const base::FilePath& v4_store_path) {
  V4StoreFileFormat v4_file_format;
  base::FilePath v5_hash_file_path;
  base::FilePath temp_store_path = store_path_.AddExtensionASCII("tmp");
  bool migration_succeeded = false;

  // If we fail to migrate from v4, we wipe the v4 files and the attempted v5
  // file format temp file.
  absl::Cleanup cleanup_on_failure = [&v4_store_path, &v4_file_format,
                                      &v5_hash_file_path, &temp_store_path,
                                      &migration_succeeded] {
    if (!migration_succeeded) {
      base::DeleteFile(v4_store_path);
      for (const auto& hash_file : v4_file_format.hash_files()) {
        base::DeleteFile(
            HashPrefixContainer::GetPath(v4_store_path, hash_file.extension()));
      }
      if (!v5_hash_file_path.empty()) {
        base::DeleteFile(v5_hash_file_path);
      }
      base::DeleteFile(temp_store_path);
    }
  };

  // Parse and validate the existing V4 store file.
  StoreReadResult validation_result =
      ParseAndValidateV4StoreFileFormat(v4_store_path, v4_file_format);
  if (validation_result != READ_SUCCESS) {
    base::UmaHistogramExactLinear(
        "SafeBrowsing.V5Store.V4ToV5Migration.V4ReadFailureReason",
        validation_result, STORE_READ_RESULT_MAX);
    return V4ToV5MigrationResult::kReadV4Failed;
  }

  // V5 store only supports a single hash file.
  if (v4_file_format.hash_files_size() > 1) {
    return V4ToV5MigrationResult::kMultipleHashFilesFailure;
  }

  std::string* v5_checksum = nullptr;
  if (v4_file_format.list_update_response().has_checksum() &&
      v4_file_format.list_update_response().checksum().has_sha256()) {
    v5_checksum = v4_file_format.mutable_list_update_response()
                      ->mutable_checksum()
                      ->mutable_sha256();
  }

  base::FilePath v4_hash_file_path;
  std::string v5_ext;
  uint64_t file_size = 0;

  // Handle the V4 hash file if it exists.
  if (v4_file_format.hash_files_size() == 1) {
    const auto& hash_file = v4_file_format.hash_files(0);
    PrefixSize v4_prefix_size = hash_file.prefix_size();
    PrefixSize expected_v4_prefix_size =
        is_extensions_blocklist_ ? 32 : prefix_size_;
    if (v4_prefix_size != expected_v4_prefix_size) {
      return V4ToV5MigrationResult::kPrefixSizeMismatchFailure;
    }
    v4_hash_file_path =
        HashPrefixContainer::GetPath(v4_store_path, hash_file.extension());
    if (!base::PathExists(v4_hash_file_path)) {
      return V4ToV5MigrationResult::kHashFileMissingFailure;
    }
    file_size = hash_file.file_size();

    // Extract the V5 extension (timestamp part) from V4 extension
    // (prefix_timestamp).
    std::string v4_ext = hash_file.extension();
    size_t underscore_pos = v4_ext.find('_');
    if (underscore_pos == std::string::npos) {
      return V4ToV5MigrationResult::kExtensionParsingFailure;
    }
    v5_ext = v4_ext.substr(underscore_pos + 1);
    if (v5_ext.empty()) {
      return V4ToV5MigrationResult::kExtensionParsingFailure;
    }
    v5_hash_file_path = HashPrefixContainer::GetPath(store_path_, v5_ext);
    // Write to the new hash file.
    if (is_extensions_blocklist_) {
      // For the extensions blocklist, migrate the length-32 extension IDs to
      // 16-byte hashes into the v5 hash file, and delete the v4 hash file.
      ConvertExtensionBlocklistV4ToV5Result result =
          ConvertExtensionsBlocklistFromV4ToV5(
              v4_hash_file_path, v5_hash_file_path, v5_checksum, &file_size);
      base::UmaHistogramEnumeration(
          "SafeBrowsing.V5Store.ConvertExtensionBlocklistV4ToV5Result", result);
      if (result != ConvertExtensionBlocklistV4ToV5Result::kSuccess) {
        return V4ToV5MigrationResult::kExtensionBlocklistMigrationFailed;
      }
      base::DeleteFile(v4_hash_file_path);
    } else {
      // For other blocklists, just rename the v4 hash file to v5.
      if (!base::Move(v4_hash_file_path, v5_hash_file_path)) {
        return V4ToV5MigrationResult::kRenameHashFileFailure;
      }
    }
  }

  // Construct the new V5StoreFileFormat proto.
  V5StoreFileFormat v5_file_format;
  v5_file_format.set_magic_number(v4_file_format.magic_number());
  v5_file_format.set_file_version(kV5FileVersion);

  ListDetails* list_details = v5_file_format.mutable_list_details();
  if (v4_file_format.list_update_response().has_new_client_state()) {
    list_details->set_version(
        v4_file_format.list_update_response().new_client_state());
  }
  if (v5_checksum) {
    list_details->mutable_checksum()->set_sha256(std::move(*v5_checksum));
  }

  if (!v5_ext.empty()) {
    V5HashFile* v5_hash_file = list_details->mutable_hash_file();
    v5_hash_file->set_extension(v5_ext);
    // TODO(crbug.com/362791941): ensure this is the same as what V5 WriteToDisk
    // eventually does
    v5_hash_file->set_file_size(file_size);
  }

  // Serialize and write the new V5 proto to disk.
  std::string v5_file_format_string;
  if (!v5_file_format.SerializeToString(&v5_file_format_string)) {
    return V4ToV5MigrationResult::kProtoSerializationFailure;
  }

  if (!base::WriteFile(temp_store_path, v5_file_format_string)) {
    return V4ToV5MigrationResult::kWriteV5FileFailure;
  }

  if (!base::Move(temp_store_path, store_path_)) {
    return V4ToV5MigrationResult::kRenameV5StoreFileFailure;
  }

  migration_succeeded = true;

  // Delete the old V4 store file.
  base::DeleteFile(v4_store_path);

  return V4ToV5MigrationResult::kV4ToV5MigrationSucceeded;
}

bool V5Store::WipeV4Store(const base::FilePath& v4_store_path) {
  V4StoreFileFormat v4_file_format;
  bool hash_delete_success = true;
  if (ParseAndValidateV4StoreFileFormat(v4_store_path, v4_file_format) ==
      READ_SUCCESS) {
    for (const auto& hash_file : v4_file_format.hash_files()) {
      base::FilePath v4_hash_file_path =
          HashPrefixContainer::GetPath(v4_store_path, hash_file.extension());
      if (!base::DeleteFile(v4_hash_file_path)) {
        hash_delete_success = false;
      }
    }
  }
  bool store_delete_success = base::DeleteFile(v4_store_path);
  return hash_delete_success && store_delete_success;
}

ConvertExtensionBlocklistV4ToV5Result
V5Store::ConvertExtensionsBlocklistFromV4ToV5(
    const base::FilePath& v4_hash_file_path,
    const base::FilePath& v5_hash_file_path,
    std::string* checksum_sha256,
    uint64_t* file_size) {
  std::string v4_data;
  if (!base::ReadFileToString(v4_hash_file_path, &v4_data)) {
    return ConvertExtensionBlocklistV4ToV5Result::kReadV4Failed;
  }

  // Verify V4 checksum if provided and not empty. For the purposes of the disk
  // migration, we don't fail when the checksum is missing, because
  // `V4Store::VerifyChecksum` allows it. `V5Store::VerifyChecksum` may end up
  // being different.
  if (checksum_sha256 && !checksum_sha256->empty()) {
    std::array<uint8_t, crypto::hash::kSha256Size> calculated_checksum;
    crypto::hash::Hash(crypto::hash::HashKind::kSha256,
                       base::as_byte_span(v4_data), calculated_checksum);
    if (checksum_sha256->size() != crypto::hash::kSha256Size ||
        base::as_byte_span(*checksum_sha256) != calculated_checksum) {
      return ConvertExtensionBlocklistV4ToV5Result::kV4ChecksumMismatch;
    }
  }
  if (v4_data.size() % 32 != 0) {
    return ConvertExtensionBlocklistV4ToV5Result::kInvalidFileSize;
  }
  std::string v5_hash_data;
  v5_hash_data.reserve(v4_data.size() / 2);
  for (size_t i = 0; i < v4_data.size(); i += 32) {
    std::string_view id = std::string_view(v4_data).substr(i, 32);
    if (!crx_file::id_util::IdIsValid(id)) {
      return ConvertExtensionBlocklistV4ToV5Result::kInvalidExtensionId;
    }
    v5_hash_data.append(SBStore::ExtensionV4IdToV5Hash(id));
  }
  if (!base::WriteFile(v5_hash_file_path, v5_hash_data)) {
    return ConvertExtensionBlocklistV4ToV5Result::kWriteV5Failed;
  }
  *file_size = v5_hash_data.size();

  if (checksum_sha256 && !checksum_sha256->empty()) {
    std::array<uint8_t, crypto::hash::kSha256Size> v5_checksum;
    crypto::hash::Hash(crypto::hash::HashKind::kSha256,
                       base::as_byte_span(v5_hash_data), v5_checksum);
    checksum_sha256->assign(v5_checksum.begin(), v5_checksum.end());
  }

  return ConvertExtensionBlocklistV4ToV5Result::kSuccess;
}

int64_t V5Store::RecordAndReturnFileSize(const std::string& base_metric) {
  // TODO(crbug.com/362791941): implement
  NOTREACHED();
}

void V5Store::Reset() {
  expected_checksum_.clear();
  hash_prefix_list_->Clear();
  version_.clear();
  has_valid_data_ = false;
  file_size_ = 0;
}

bool V5Store::VerifyChecksum() {
  base::ElapsedThreadTimer thread_timer;
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!has_valid_data_) {
    // No need to verify the checksum because the store is already slated to
    // get a full update.
    return true;
  }
  bool checksum_matches = VerifyChecksumInternal();
  RecordApplyUpdateResult("SafeBrowsing.V5ReadFromDisk",
                          checksum_matches
                              ? V5ApplyUpdateResult::kSuccess
                              : V5ApplyUpdateResult::kChecksumMismatchFailure,
                          store_path_);
  base::TimeDelta duration = thread_timer.Elapsed();
  base::UmaHistogramTimes("SafeBrowsing.V5ReadFromDisk.VerifyChecksumDuration",
                          duration);
  base::UmaHistogramTimes("SafeBrowsing.SBReadFromDisk.VerifyChecksumDuration",
                          duration);
  return checksum_matches;
}

bool V5Store::VerifyChecksumInternal() {
  const HashPrefixMapView map_view = hash_prefix_list_->view();
  CHECK_LE(map_view.size(), 1u);

  base::span<const uint8_t> data_for_checksum =
      (!map_view.empty() && !map_view.begin()->second.empty())
          ? base::as_byte_span(map_view.begin()->second)
          : base::span<const uint8_t>();
  auto checksum = crypto::hash::Sha256(data_for_checksum);
  return base::as_byte_span(expected_checksum_) == checksum;
}

void V5Store::CollectStoreInfo(
    DatabaseManagerInfo::DatabaseInfo::StoreInfo* store_info) {
  // TODO(crbug.com/362791941): implement
  NOTREACHED();
}

HashPrefixStr V5Store::GetMatchingHashPrefix(const FullHashStr& full_hash) {
  // TODO(crbug.com/362791941): implement
  NOTREACHED();
}

void V5Store::ApplyUpdate(
    std::unique_ptr<SBUpdateResponse> response,
    const scoped_refptr<base::SequencedTaskRunner>& runner,
    UpdatedStoreReadyCallback callback) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  CHECK(response->v5_response);

  std::unique_ptr<V5::HashList> v5_response = std::move(response->v5_response);
  base::ElapsedThreadTimer thread_timer;

  bool is_full_update = !v5_response->partial_update();
  std::string metric = is_full_update ? "SafeBrowsing.V5ProcessFullUpdate"
                                      : "SafeBrowsing.V5ProcessPartialUpdate";

  base::expected<SBStorePtr, V5ApplyUpdateResult> update_result =
      ApplyUpdateInternal(std::move(v5_response), metric);

  V5ApplyUpdateResult result_code = update_result.has_value()
                                        ? V5ApplyUpdateResult::kSuccess
                                        : update_result.error();
  SBStorePtr new_store =
      update_result.has_value()
          ? std::move(update_result.value())
          : SBStorePtr(nullptr, SBStoreDeleter(task_runner_));

  // Record the state of the update to be shown in the Safe Browsing page.
  last_apply_update_result_ = result_code;

  RecordApplyUpdateResult(metric, result_code, store_path_);
  RecordApplyUpdateDuration(metric, thread_timer.Elapsed());

  // Posting the task should be the last thing to do in this function.
  // Otherwise, the posted task can end up running in parallel. If that
  // happens, the old store will get destroyed and can lead to use-after-free
  // in this function.
  runner->PostTask(FROM_HERE,
                   base::BindOnce(std::move(callback), std::move(new_store)));
}

base::expected<SBStorePtr, V5ApplyUpdateResult> V5Store::ApplyUpdateInternal(
    std::unique_ptr<V5::HashList> v5_response,
    const std::string& metric) {
  V5StorePtr new_store(new V5Store(task_runner_, store_path_, prefix_size_,
                                   v4_store_path_, is_eligible_for_migration_,
                                   is_extensions_blocklist_, file_size_),
                       SBStoreDeleter(task_runner_));
  new_store->expected_checksum_ = expected_checksum_;

  bool is_full_update = !v5_response->partial_update();
  HashPrefixesView old_prefixes =
      is_full_update ? HashPrefixesView() : hash_prefix_list_->GetRawView();
  V5ApplyUpdateResult apply_update_result = new_store->ProcessUpdate(
      std::move(v5_response), metric, is_full_update, old_prefixes);

  if (apply_update_result != V5ApplyUpdateResult::kSuccess) {
    return base::unexpected(apply_update_result);
  }

  V5StoreWriteResult write_result = new_store->WriteToDisk();
  RecordStoreWriteResult(write_result);
  if (write_result != V5StoreWriteResult::kWriteSuccess) {
    return base::unexpected(V5ApplyUpdateResult::kWriteFailure);
  }

  apply_update_result = new_store->hash_prefix_list_->IsValid();
  if (apply_update_result != V5ApplyUpdateResult::kSuccess) {
    return base::unexpected(apply_update_result);
  }

  new_store->has_valid_data_ = true;
  new_store->last_apply_update_result_ = apply_update_result;

  return std::move(new_store);
}

V5ApplyUpdateResult V5Store::ProcessUpdate(
    std::unique_ptr<V5::HashList> response,
    const std::string& metric,
    bool is_full_update,
    HashPrefixesView old_prefixes_list) {
  // Decode removals.
  std::vector<uint32_t> removals;
  V5DecodeResult decode_removals_result =
      v5_hash_list_rice_decoder::DecodeRemovals(*response, removals);
  RecordDecodeRemovalsResult(metric, decode_removals_result, store_path_);
  if (decode_removals_result != V5DecodeResult::kSuccess) {
    return V5ApplyUpdateResult::kRiceDecodingRemovalsFailure;
  }
  RecordRemovalsHashesCount(metric, removals.size(), store_path_);

  // Decode additions.
  std::string additions;
  V5DecodeResult decode_additions_result =
      v5_hash_list_rice_decoder::DecodeAdditions(*response, additions);
  RecordDecodeAdditionsResult(metric, decode_additions_result, store_path_);
  if (decode_additions_result != V5DecodeResult::kSuccess) {
    return V5ApplyUpdateResult::kRiceDecodingAdditionsFailure;
  }
  RecordAdditionsHashesCount(metric, is_full_update,
                             additions.size() / prefix_size_, store_path_);

  // Merge the additions and removals into the existing list.
  base::span<const uint8_t> old_prefixes =
      base::as_byte_span(old_prefixes_list);
  base::span<const uint8_t> new_prefixes = base::as_byte_span(additions);
  CHECK_EQ(new_prefixes.size() % prefix_size_, 0u);

  if (!response->sha256_checksum().empty()) {
    expected_checksum_ = response->sha256_checksum();
  }
  // TODO(crbug.com/372395685): This can move into merging updates once v4
  // implementation is deprecated.
  if (expected_checksum_.empty()) {
    return V5ApplyUpdateResult::kChecksumMismatchFailure;
  }

  SBStoreUpdateResult result =
      MergeUpdateLoop(prefix_size_, old_prefixes, new_prefixes, &removals,
                      expected_checksum_, hash_prefix_list_.get());

  switch (result) {
    case SBStoreUpdateResult::kSuccess:
      version_ = response->version();
      return V5ApplyUpdateResult::kSuccess;
    case SBStoreUpdateResult::kAdditionsHasExistingPrefixFailure:
      return V5ApplyUpdateResult::kAdditionsHasExistingPrefixFailure;
    case SBStoreUpdateResult::kRemovalsIndexTooLargeFailure:
      return V5ApplyUpdateResult::kRemovalsIndexTooLargeFailure;
    case SBStoreUpdateResult::kChecksumMismatchFailure:
      return V5ApplyUpdateResult::kChecksumMismatchFailure;
  }
}

const std::string& V5Store::GetStoreState() const {
  return version_;
}

V5StoreWriteResult V5Store::WriteToDisk() {
  V5StoreFileFormat file_format;

  base::expected<int64_t, SBStoreWriteResult> file_size_or_error =
      WriteToDiskLoop(
          store_path_, &file_format, hash_prefix_list_.get(),
          /*set_file_metadata=*/
          [this, &file_format] {
            file_format.set_magic_number(kFileMagic);
            file_format.set_file_version(kV5FileVersion);
            ListDetails* list_details = file_format.mutable_list_details();
            list_details->set_version(version_);
            if (!expected_checksum_.empty()) {
              list_details->mutable_checksum()->set_sha256(expected_checksum_);
            }
          },
          /*cleanup_on_error=*/
          [this, &file_format](const base::FilePath& temp_file) {
            // Clear the list to release any memory-mapped files. This is
            // required to allow the files to be deleted on Windows, where open
            // or mapped mapped files cannot be deleted.
            hash_prefix_list_->Clear();
            base::DeleteFile(temp_file);
            if (file_format.list_details().has_hash_file()) {
              base::DeleteFile(HashPrefixContainer::GetPath(
                  store_path_,
                  file_format.list_details().hash_file().extension()));
            }
          },
          /*get_hash_files_size=*/
          [&file_format]() -> int64_t {
            return file_format.list_details().has_hash_file()
                       ? file_format.list_details().hash_file().file_size()
                       : 0;
          },
          /*cleanup_extra_files=*/
          [this, &file_format] {
            CleanupExtraFiles(store_path_, file_format);
          });

  if (file_size_or_error.has_value()) {
    // Update file_size_ now because we wrote the file correctly.
    file_size_ = file_size_or_error.value();
    return V5StoreWriteResult::kWriteSuccess;
  }

  switch (file_size_or_error.error()) {
    case SBStoreWriteResult::kUnexpectedBytesWrittenFailure:
      return V5StoreWriteResult::kUnexpectedBytesWrittenFailure;
    case SBStoreWriteResult::kUnexpectedWriteFailure:
      return V5StoreWriteResult::kUnexpectedWriteFailure;
    case SBStoreWriteResult::kUnableToRenameFailure:
      return V5StoreWriteResult::kUnableToRenameFailure;
  }
}

SBStorePtr V5StoreFactory::CreateStore(
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
    const base::FilePath& base_path,
    const ListInfo& list_info) {
  const base::FilePath store_path = base_path.AppendASCII(list_info.filename());
  const base::FilePath v4_store_path =
      base_path.AppendASCII(list_info.v4_filename());

  return CreateV5Store(
      db_task_runner, store_path, list_info.v5_prefix_size().value(),
      v4_store_path,
      /*is_eligible_for_v4_to_v5_disk_migration=*/list_info.list_id() !=
          GetUrlCsdAllowlistId(),
      /*is_extensions_blocklist=*/list_info.list_id() ==
          GetChromeExtMalwareId());
}

V5StorePtr V5StoreFactory::CreateV5Store(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const base::FilePath& store_path,
    PrefixSize prefix_size,
    const base::FilePath& v4_store_path,
    bool is_eligible_for_v4_to_v5_disk_migration,
    bool is_extensions_blocklist) {
  V5StorePtr new_store(
      new V5Store(task_runner, store_path, prefix_size, v4_store_path,
                  is_eligible_for_v4_to_v5_disk_migration,
                  is_extensions_blocklist),
      SBStoreDeleter(task_runner));
  new_store->Initialize();
  return new_store;
}

}  // namespace safe_browsing
