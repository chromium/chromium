// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/sb_store.h"

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "components/crx_file/id_util.h"
#include "components/safe_browsing/core/browser/db/sb_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/v4_store.pb.h"
#include "components/safe_browsing/core/common/proto/v5_store.pb.h"

namespace safe_browsing {

SBStore::SBStore(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                 const base::FilePath& store_path,
                 int64_t old_file_size)
    : file_size_(old_file_size),
      has_valid_data_(false),
      store_path_(store_path),
      task_runner_(task_runner) {}

SBStore::~SBStore() = default;

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

}  // namespace safe_browsing
