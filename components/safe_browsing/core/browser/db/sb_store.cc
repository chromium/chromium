// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/sb_store.h"

#include <optional>
#include <vector>

#include "base/base64.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "components/crx_file/id_util.h"
#include "components/safe_browsing/core/browser/db/hash_prefix_container.h"
#include "components/safe_browsing/core/browser/db/hash_prefix_list.h"
#include "components/safe_browsing/core/browser/db/hash_prefix_map.h"
#include "components/safe_browsing/core/browser/db/sb_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/v4_store.pb.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"
#include "components/safe_browsing/core/common/proto/v5_store.pb.h"
#include "crypto/hash.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

namespace safe_browsing {

SBStore::SBStore(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                 const base::FilePath& store_path,
                 int64_t old_file_size)
    : file_size_(old_file_size),
      has_valid_data_(false),
      store_path_(store_path),
      task_runner_(task_runner) {}

SBStore::~SBStore() = default;

void SBStore::CollectStoreInfo(
    DatabaseManagerInfo::DatabaseInfo::StoreInfo* store_info) {
  store_info->set_file_name(GetUmaSuffixForStore(store_path_)
                                .substr(1));  // Strip the '.' off the front
  store_info->set_file_size_bytes(file_size_);
  store_info->set_state(GetStoreState());
  if (last_apply_update_time_millis_.InMillisecondsSinceUnixEpoch()) {
    store_info->set_last_apply_update_time_millis(
        last_apply_update_time_millis_.InMillisecondsSinceUnixEpoch());
  }
}

bool SBStore::HasValidData() {
  // Record every 256th time (`record_has_valid_data_counter_` is 8-bit).
  if (++record_has_valid_data_counter_ == 1) {
    LogHasValidDataHistograms();
  }
  return has_valid_data_;
}

void SBStore::LogHasValidDataHistograms() {
  std::string suffix = GetUmaSuffixForStore(store_path_);
  std::string sb_store_suffix = suffix;
  // Make sure that the SBStore suffix does not have "_v5" at the end, that way
  // the SBStore logs are directly comparable between v4 and v5.
  // TODO(crbug.com/362791941): Pull out a shared constant for "_v5".
  if (base::EndsWith(sb_store_suffix, "_v5", base::CompareCase::SENSITIVE)) {
    sb_store_suffix = sb_store_suffix.substr(0, sb_store_suffix.length() - 3);
  }
  RecordBooleanWithAndWithoutSuffix("SafeBrowsing.SBStore.IsStoreValid",
                                    has_valid_data_, sb_store_suffix);
  RecordBooleanWithAndWithoutSuffix(GetMetricPrefix() + ".IsStoreValid",
                                    has_valid_data_, suffix);
}

// static
void SBStore::RecordBooleanWithAndWithoutSuffix(const std::string& metric,
                                                bool value,
                                                const std::string& suffix) {
  base::UmaHistogramBoolean(metric, value);
  base::UmaHistogramBoolean(metric + suffix, value);
}

BaseFileInputStream::BaseFileInputStream(const base::FilePath& input_file)
    : stream_(input_file), impl_(&stream_) {}

BaseFileInputStream::~BaseFileInputStream() = default;

base::File::Error BaseFileInputStream::GetError() const {
  return stream_.GetError();
}

bool BaseFileInputStream::Next(const void** data, int* size) {
  return impl_.Next(data, size);
}

void BaseFileInputStream::BackUp(int count) {
  return impl_.BackUp(count);
}

bool BaseFileInputStream::Skip(int count) {
  return impl_.Skip(count);
}

int64_t BaseFileInputStream::ByteCount() const {
  return impl_.ByteCount();
}

BaseFileInputStream::CopyingBaseFileInputStream::CopyingBaseFileInputStream(
    const base::FilePath& input_file)
    : file_(input_file,
            base::File::FLAG_OPEN | base::File::FLAG_READ |
                base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                base::File::FLAG_WIN_SHARE_DELETE) {}

BaseFileInputStream::CopyingBaseFileInputStream::~CopyingBaseFileInputStream() =
    default;

base::File::Error BaseFileInputStream::CopyingBaseFileInputStream::GetError()
    const {
  return file_.error_details();
}

int BaseFileInputStream::CopyingBaseFileInputStream::Read(void* buffer,
                                                          int size) {
  if (!file_.IsValid()) {
    return -1;
  }
  const std::optional<size_t> bytes_read = file_.ReadAtCurrentPos(
      UNSAFE_TODO(base::span(reinterpret_cast<uint8_t*>(buffer),
                             base::checked_cast<size_t>(size))));
  if (bytes_read) {
    return base::checked_cast<int>(*bytes_read);
  }
  file_ = base::File(base::File::GetLastFileError());
  return -1;
}

int BaseFileInputStream::CopyingBaseFileInputStream::Skip(int count) {
  if (file_.Seek(base::File::FROM_CURRENT, count) != -1) {
    return count;
  }
  return CopyingInputStream::Skip(count);
}

// static
StoreReadResult SBStore::ParseAndValidateV4StoreFileFormat(
    const base::FilePath& store_path,
    V4StoreFileFormat& file_format,
    int64_t* file_size) {
  {
    BaseFileInputStream input_stream(store_path);
    if (!file_format.ParseFromZeroCopyStream(&input_stream)) {
      return input_stream.GetError() != base::File::FILE_OK
                 ? FILE_UNREADABLE_FAILURE
                 : PROTO_PARSING_FAILURE;
    }
    // `ParseFromZeroCopyStream` will return true if the file didn't exist, so
    // explicitly check for an error when reading from the file.
    if (input_stream.GetError() != base::File::FILE_OK) {
      return FILE_UNREADABLE_FAILURE;
    }
    int64_t bytes_read = input_stream.ByteCount();
    if (!bytes_read) {
      return FILE_EMPTY_FAILURE;
    }
    if (file_size) {
      *file_size = bytes_read;
    }
  }

  if (file_format.magic_number() != kFileMagic) {
    return UNEXPECTED_MAGIC_NUMBER_FAILURE;
  }

  if (file_format.version_number() != kV4FileVersion) {
    return FILE_VERSION_INCOMPATIBLE_FAILURE;
  }

  if (!file_format.has_list_update_response()) {
    return HASH_PREFIX_INFO_MISSING_FAILURE;
  }

  if (!file_format.list_update_response().additions().empty()) {
    return PRE_MMAP_MIGRATION_FILE_FORMAT_FAILURE;
  }

  return READ_SUCCESS;
}

// static
V5StoreReadResult SBStore::ParseAndValidateV5StoreFileFormat(
    const base::FilePath& store_path,
    V5StoreFileFormat& file_format,
    int64_t* file_size) {
  {
    BaseFileInputStream input_stream(store_path);
    if (!file_format.ParseFromZeroCopyStream(&input_stream)) {
      return input_stream.GetError() != base::File::FILE_OK
                 ? V5StoreReadResult::kFileReadFailure
                 : V5StoreReadResult::kProtoParsingFailure;
    }
    // `ParseFromZeroCopyStream` will return true if the file didn't exist, so
    // explicitly check for an error when reading from the file.
    if (input_stream.GetError() != base::File::FILE_OK) {
      return V5StoreReadResult::kFileOpenFailure;
    }
    int64_t bytes_read = input_stream.ByteCount();
    if (!bytes_read) {
      return V5StoreReadResult::kFileEmptyFailure;
    }
    if (file_size) {
      *file_size = bytes_read;
    }
  }

  if (file_format.magic_number() != kFileMagic) {
    return V5StoreReadResult::kUnexpectedMagicNumberFailure;
  }

  if (file_format.file_version() != kV5FileVersion) {
    return V5StoreReadResult::kFileVersionIncompatibleFailure;
  }

  if (!file_format.has_list_details()) {
    return V5StoreReadResult::kHashPrefixInfoMissingFailure;
  }

  return V5StoreReadResult::kReadSuccess;
}

// static
std::string SBStore::ExtensionV4IdToV5Hash(std::string_view v4_id) {
  CHECK_EQ(v4_id.size(), 32u);
  CHECK(crx_file::id_util::IdIsValid(v4_id));
  std::string v5_hash;
  v5_hash.reserve(16);
  for (size_t i = 0; i < 32; i += 2) {
    uint8_t val1 = base::ToLowerASCII(v4_id[i]) - 'a';
    uint8_t val2 = base::ToLowerASCII(v4_id[i + 1]) - 'a';
    v5_hash.push_back(static_cast<char>((val1 << 4) | val2));
  }
  return v5_hash;
}

SBUpdateResponse::SBUpdateResponse() = default;
SBUpdateResponse::~SBUpdateResponse() = default;

SBStoreDeleter::SBStoreDeleter(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}
SBStoreDeleter::~SBStoreDeleter() = default;
SBStoreDeleter::SBStoreDeleter(SBStoreDeleter&&) = default;
SBStoreDeleter& SBStoreDeleter::operator=(SBStoreDeleter&&) = default;

BaseFileOutputStream::BaseFileOutputStream(const base::FilePath& output_file)
    : CopyingOutputStreamAdaptor(&stream_), stream_(output_file) {}

BaseFileOutputStream::~BaseFileOutputStream() = default;

base::File::Error BaseFileOutputStream::GetError() const {
  return stream_.GetError();
}

BaseFileOutputStream::CopyingBaseFileOutputStream::CopyingBaseFileOutputStream(
    const base::FilePath& output_file)
    : file_(output_file,
            base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE |
                base::File::FLAG_WIN_EXCLUSIVE_READ |
                base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                base::File::FLAG_WIN_SHARE_DELETE) {}

BaseFileOutputStream::CopyingBaseFileOutputStream::
    ~CopyingBaseFileOutputStream() = default;

base::File::Error BaseFileOutputStream::CopyingBaseFileOutputStream::GetError()
    const {
  return file_.error_details();
}

bool BaseFileOutputStream::CopyingBaseFileOutputStream::Write(
    const void* buffer,
    int size) {
  if (!file_.IsValid()) {
    return false;
  }
  std::optional<size_t> bytes_written = file_.WriteAtCurrentPos(
      UNSAFE_TODO(base::span(reinterpret_cast<const uint8_t*>(buffer),
                             base::checked_cast<size_t>(size))));
  if (bytes_written == base::checked_cast<size_t>(size)) {
    return true;
  }
  file_ = base::File(base::File::GetLastFileError());
  return false;
}

// static
template <typename RemovalsContainer>
SBStoreUpdateResult SBStore::MergeUpdateLoop(
    PrefixSize prefix_size,
    base::span<const uint8_t> old_prefixes,
    base::span<const uint8_t> new_prefixes,
    const RemovalsContainer* raw_removals,
    const std::string& expected_checksum,
    HashPrefixContainer* out_container) {
  std::optional<typename RemovalsContainer::const_iterator> removals_iter =
      raw_removals ? std::make_optional(raw_removals->begin()) : std::nullopt;

  // Keep track of the number of elements picked from the old map. This is used
  // to determine which elements to drop based on the raw_removals. Note that
  // picked is not the same as merged. A picked element isn't merged if its
  // index is on the raw_removals list.
  uint32_t total_picked_from_old = 0;

  crypto::hash::Hasher checksum_ctx(crypto::hash::HashKind::kSha256);
  const bool calculate_checksum = !expected_checksum.empty();

  // Look ahead in `prefixes` to see how many sequential prefixes are lower than
  // `limit_prefix`, the next prefix from the other list. This uses a binary
  // search.
  auto get_skip_count = [&](base::span<const uint8_t> prefixes,
                            base::span<const uint8_t> limit_prefix) {
    size_t num_prefixes = prefixes.size() / prefix_size;
    if (num_prefixes < 2 ||
        prefixes.subspan(prefix_size, prefix_size) >= limit_prefix) {
      return size_t{1};
    }

    size_t left = 2;
    size_t right = num_prefixes;
    size_t run_length = 1;
    while (left <= right) {
      size_t mid = left + (right - left) / 2;
      if (prefixes.subspan((mid - 1) * prefix_size, prefix_size) <
          limit_prefix) {
        run_length = mid;
        left = mid + 1;
      } else {
        right = mid - 1;
      }
    }
    return run_length;
  };

  while (!old_prefixes.empty() || !new_prefixes.empty()) {
    bool pick_from_old = !old_prefixes.empty();
    if (pick_from_old && !new_prefixes.empty()) {
      base::span<const uint8_t> old_prefix = old_prefixes.first(prefix_size);
      base::span<const uint8_t> add_prefix = new_prefixes.first(prefix_size);
      if (old_prefix == add_prefix) {
        return SBStoreUpdateResult::kAdditionsHasExistingPrefixFailure;
      }
      pick_from_old = old_prefix < add_prefix;
    }

    if (pick_from_old) {
      // Should this index be removed?
      if (removals_iter && *removals_iter != raw_removals->end() &&
          base::checked_cast<uint32_t>(**removals_iter) ==
              total_picked_from_old) {
        old_prefixes.take_first(prefix_size);
        total_picked_from_old++;
        (*removals_iter)++;
        continue;
      }

      // We can only scan ahead up to the next removal index.
      size_t count = old_prefixes.size() / prefix_size;
      if (removals_iter && *removals_iter != raw_removals->end()) {
        count = std::min(count, base::checked_cast<size_t>(
                                    **removals_iter - total_picked_from_old));
      }

      // Determine how many prefixes we can take in a row from this list by
      // scanning ahead.
      if (!new_prefixes.empty()) {
        base::span<const uint8_t> add_prefix = new_prefixes.first(prefix_size);
        count = std::min(count,
                         get_skip_count(old_prefixes.first(count * prefix_size),
                                        add_prefix));
      }
      CHECK_GE(count, 1u);

      // Append the selected prefixes.
      base::span<const uint8_t> to_append =
          old_prefixes.take_first(count * prefix_size);
      out_container->Append(prefix_size, base::as_string_view(to_append));
      if (calculate_checksum) {
        checksum_ctx.Update(to_append);
      }
      total_picked_from_old += count;
      continue;
    }

    // Picking from `new_prefixes`.
    size_t count = new_prefixes.size() / prefix_size;

    // Scan ahead.
    if (!old_prefixes.empty()) {
      base::span<const uint8_t> old_prefix = old_prefixes.first(prefix_size);
      count = std::min(count, get_skip_count(new_prefixes, old_prefix));
    }
    CHECK_GE(count, 1u);

    // Append the selected prefixes.
    base::span<const uint8_t> to_append =
        new_prefixes.take_first(count * prefix_size);
    out_container->Append(prefix_size, base::as_string_view(to_append));
    if (calculate_checksum) {
      checksum_ctx.Update(to_append);
    }
  }

  if (removals_iter && *removals_iter != raw_removals->end()) {
    return SBStoreUpdateResult::kRemovalsIndexTooLargeFailure;
  }

  if (calculate_checksum) {
    std::array<uint8_t, crypto::hash::kSha256Size> checksum;
    checksum_ctx.Finish(checksum);
    auto expected = base::as_byte_span(expected_checksum);
    if (expected != checksum) {
#if DCHECK_IS_ON()
      std::string checksum_b64 = base::Base64Encode(checksum);
      std::string expected_b64 = base::Base64Encode(expected);
      DVLOG(1) << "Failure: Checksum mismatch: calculated: " << checksum_b64
               << "; expected: " << expected_b64;
#endif
      return SBStoreUpdateResult::kChecksumMismatchFailure;
    }
  }

  return SBStoreUpdateResult::kSuccess;
}

// static
template <typename FileFormat, typename Container>
base::expected<int64_t, SBStoreWriteResult> SBStore::WriteToDiskLoop(
    const base::FilePath& store_path,
    FileFormat* file_format,
    Container* container,
    base::FunctionRef<void()> set_file_metadata,
    base::FunctionRef<void(const base::FilePath&)> cleanup_on_error,
    base::FunctionRef<int64_t()> get_hash_files_size,
    base::FunctionRef<void()> cleanup_extra_files) {
  // Attempt writing to a temporary file first and at the end, swap the files.
  const base::FilePath new_filename = TemporaryFileForFilename(store_path);

  absl::Cleanup cleanup_on_error_block = [&new_filename, &cleanup_on_error] {
    cleanup_on_error(new_filename);
  };

  int64_t written = 0;
  // `write_session` must remain alive until `file_format` is committed to disk.
  // Additionally, note that `container` is unusable throughout the
  // lifetime of `write_session`.
  SBStoreFileFormat sb_file_format(file_format);
  if (auto write_session = container->WriteToDisk(sb_file_format);
      write_session) {
    set_file_metadata();
    BaseFileOutputStream output_stream(new_filename);
    if (!file_format->SerializeToZeroCopyStream(&output_stream) ||
        !output_stream.Flush()) {
      return base::unexpected(
          SBStoreWriteResult::kUnexpectedBytesWrittenFailure);
    }
    written = output_stream.ByteCount();
  } else {
    return base::unexpected(SBStoreWriteResult::kUnexpectedWriteFailure);
  }

  if (!base::Move(new_filename, store_path)) {
    return base::unexpected(SBStoreWriteResult::kUnableToRenameFailure);
  }

  // Calculate the new total file size based on bytes written correctly.
  int64_t file_size = written + get_hash_files_size();

  // No cleanup needed, cancel the cleanup.
  std::move(cleanup_on_error_block).Cancel();
  cleanup_extra_files();

  return file_size;
}

// static
const base::FilePath SBStore::TemporaryFileForFilename(
    const base::FilePath& filename) {
  return base::FilePath(filename.value() + FILE_PATH_LITERAL("_new"));
}

// Explicit instantiations.
template SBStoreUpdateResult SBStore::MergeUpdateLoop(
    PrefixSize prefix_size,
    base::span<const uint8_t> old_prefixes,
    base::span<const uint8_t> new_prefixes,
    const google::protobuf::RepeatedField<int32_t>* raw_removals,
    const std::string& expected_checksum,
    HashPrefixContainer* out_container);
template SBStoreUpdateResult SBStore::MergeUpdateLoop(
    PrefixSize prefix_size,
    base::span<const uint8_t> old_prefixes,
    base::span<const uint8_t> new_prefixes,
    const std::vector<uint32_t>* raw_removals,
    const std::string& expected_checksum,
    HashPrefixContainer* out_container);

template base::expected<int64_t, SBStoreWriteResult> SBStore::WriteToDiskLoop(
    const base::FilePath& store_path,
    V4StoreFileFormat* file_format,
    HashPrefixMap* container,
    base::FunctionRef<void()> set_file_metadata,
    base::FunctionRef<void(const base::FilePath&)> cleanup_on_error,
    base::FunctionRef<int64_t()> get_hash_files_size,
    base::FunctionRef<void()> cleanup_extra_files);
template base::expected<int64_t, SBStoreWriteResult> SBStore::WriteToDiskLoop(
    const base::FilePath& store_path,
    V5StoreFileFormat* file_format,
    HashPrefixList* container,
    base::FunctionRef<void()> set_file_metadata,
    base::FunctionRef<void(const base::FilePath&)> cleanup_on_error,
    base::FunctionRef<int64_t()> get_hash_files_size,
    base::FunctionRef<void()> cleanup_extra_files);

}  // namespace safe_browsing
