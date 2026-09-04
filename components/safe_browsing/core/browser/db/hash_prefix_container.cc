// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/hash_prefix_container.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/debug/crash_logging.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_view_util.h"
#include "components/safe_browsing/core/browser/db/prefix_iterator.h"

namespace safe_browsing {

namespace {

// Returns true if |hash_prefix| with PrefixSize |size| exists in |prefixes|.
bool HashPrefixMatches(std::string_view prefix,
                       HashPrefixesView prefixes,
                       PrefixSize size,
                       size_t start,
                       size_t end) {
  return std::binary_search(PrefixIterator(prefixes, start, size),
                            PrefixIterator(prefixes, end, size), prefix);
}

void LogWriteError(HashPrefixContainer::WriteError error,
                   std::string_view metric_prefix) {
  base::UmaHistogramEnumeration(
      base::StrCat({"SafeBrowsing.", metric_prefix, "WriteError"}), error);
  base::UmaHistogramEnumeration("SafeBrowsing.SBStoreWriteError", error);
}

}  // namespace

// Writes a hash prefix file, and buffers writes to avoid a write call for each
// hash prefix. The file will be deleted if Finish() is never called.
HashPrefixContainer::BufferedFileWriter::BufferedFileWriter(
    const base::FilePath& store_path,
    PrefixSize prefix_size,
    size_t buffer_size,
    const std::string& extension,
    std::string_view metric_prefix)
    : extension_(extension),
      path_(GetPath(store_path, extension_)),
      prefix_size_(prefix_size),
      buffer_size_(buffer_size),
      file_(path_, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE),
      has_error_(!file_.IsValid()),
      metric_prefix_(metric_prefix) {
  if (has_error_) {
    base::UmaHistogramExactLinear(
        base::StrCat({"SafeBrowsing.", metric_prefix_, "FileOpenError"}),
        -file_.error_details(), -base::File::FILE_ERROR_MAX);
    base::UmaHistogramExactLinear("SafeBrowsing.SBStoreFileOpenError",
                                  -file_.error_details(),
                                  -base::File::FILE_ERROR_MAX);
  }
  buffer_.reserve(buffer_size);
}

HashPrefixContainer::BufferedFileWriter::~BufferedFileWriter() {
  // File was never finished, delete now.
  if (file_.IsValid() || has_error_) {
    file_.Close();
    base::DeleteFile(path_);
  }
}

void HashPrefixContainer::BufferedFileWriter::Write(HashPrefixesView data) {
  if (has_error_) {
    return;
  }

  cur_size_ += data.size();

  if (buffer_.size() + data.size() >= buffer_size_) {
    Flush();
  }

  if (data.size() > buffer_size_) {
    WriteToFile(data);
  } else {
    buffer_.append(data);
  }
}

bool HashPrefixContainer::BufferedFileWriter::Finish() {
  Flush();
  file_.Close();
  return !has_error_ && cur_size_ % prefix_size_ == 0;
}

size_t HashPrefixContainer::BufferedFileWriter::GetFileSize() const {
  return cur_size_;
}

const std::string& HashPrefixContainer::BufferedFileWriter::extension() const {
  return extension_;
}

bool HashPrefixContainer::BufferedFileWriter::has_error() const {
  return has_error_;
}

void HashPrefixContainer::BufferedFileWriter::Flush() {
  WriteToFile(buffer_);
  buffer_.clear();
}

void HashPrefixContainer::BufferedFileWriter::WriteToFile(
    HashPrefixesView data) {
  if (has_error_ || data.empty()) {
    return;
  }

  size_t bytes_written = 0;
  while (bytes_written < data.size()) {
    std::optional<size_t> result =
        file_.WriteAtCurrentPos(base::as_byte_span(data.substr(bytes_written)));
    if (!result.has_value()) {
      has_error_ = true;
      base::UmaHistogramExactLinear(
          base::StrCat({"SafeBrowsing.", metric_prefix_, "FileWriteError"}),
          -base::File::GetLastFileError(), -base::File::FILE_ERROR_MAX);
      base::UmaHistogramExactLinear("SafeBrowsing.SBStoreFileWriteError",
                                    -base::File::GetLastFileError(),
                                    -base::File::FILE_ERROR_MAX);
      break;
    }
    bytes_written += *result;
  }
}

HashPrefixContainer::FileInfo::FileInfo(const base::FilePath& store_path,
                                        PrefixSize size)
    : store_path_(store_path), prefix_size_(size) {}

HashPrefixContainer::FileInfo::~FileInfo() = default;

HashPrefixesView HashPrefixContainer::FileInfo::GetView() const {
  CHECK(IsReadable());
  return base::as_string_view(file_.bytes());
}

bool HashPrefixContainer::FileInfo::Initialize(const std::string& extension,
                                               uint64_t expected_file_size,
                                               bool initialize_after_write,
                                               std::string_view metric_prefix) {
  extension_ = extension;
  // Make sure file size is correct before attempting to mmap.
  base::FilePath path = HashPrefixContainer::GetPath(store_path_, extension);
  std::optional<int64_t> actual_file_size = base::GetFileSize(path);
  if (!actual_file_size.has_value()) {
    if (initialize_after_write) {
      LogWriteError(WriteError::kFileNotFound, metric_prefix);
    }
    return false;
  }
  if (static_cast<uint64_t>(actual_file_size.value()) != expected_file_size) {
    if (initialize_after_write) {
      LogWriteError(WriteError::kFileSizeMismatch, metric_prefix);
    }
    return false;
  }

  if (IsReadable()) {
    CHECK_EQ(file_.bytes().size(), expected_file_size);
    return true;
  }

  if (!file_.Initialize(path)) {
    if (initialize_after_write) {
      LogWriteError(WriteError::kFailedMmap, metric_prefix);
    }
    return false;
  }

  if (file_.bytes().size() != static_cast<size_t>(actual_file_size.value())) {
    if (initialize_after_write) {
      LogWriteError(WriteError::kMmapSizeMismatch, metric_prefix);
    }
    return false;
  }

  return true;
}

std::optional<HashPrefixContainer::FinalizedFileInfo>
HashPrefixContainer::FileInfo::Finalize(std::string_view metric_prefix) {
  if (!writer_->Finish()) {
    if (writer_->has_error()) {
      LogWriteError(WriteError::kFileWriteError, metric_prefix);
    } else if (writer_->GetFileSize() % prefix_size_ != 0) {
      LogWriteError(WriteError::kInvalidTotalSize, metric_prefix);
    }
    return std::nullopt;
  }

  FinalizedFileInfo info{.extension = writer_->extension(),
                         .file_size = writer_->GetFileSize()};
  writer_.reset();
  return info;
}

HashPrefixStr HashPrefixContainer::FileInfo::Matches(
    std::string_view full_hash) const {
  HashPrefixStr hash_prefix(full_hash.substr(0, prefix_size_));
  HashPrefixesView prefixes = GetView();

  uint32_t start = 0;
  uint32_t end = prefixes.size() / prefix_size_;

  // If the start is the same as end, the hash doesn't exist.
  if (start == end) {
    return HashPrefixStr();
  }

  // TODO(crbug.com/40062772): Remove crash logging.
  std::string_view start_prefix = prefixes.substr(0, prefix_size_);
  std::string_view end_prefix =
      prefixes.substr(prefix_size_ * (end - 1), prefix_size_);
  SCOPED_CRASH_KEY_STRING64(
      "SafeBrowsing", "prefix_match",
      base::StrCat({base::NumberToString(start), ":", base::NumberToString(end),
                    ":", base::NumberToString(prefix_size_), ":",
                    base::NumberToString(prefixes.size()), ":",
                    base::NumberToString(start_prefix.compare(hash_prefix)),
                    ":",
                    base::NumberToString(end_prefix.compare(hash_prefix))}));

  if (HashPrefixMatches(hash_prefix, prefixes, prefix_size_, start, end)) {
    return hash_prefix;
  }
  return HashPrefixStr();
}

HashPrefixContainer::BufferedFileWriter*
HashPrefixContainer::FileInfo::GetOrCreateWriter(
    size_t buffer_size,
    const std::string& extension,
    std::string_view metric_prefix) {
  CHECK(!file_.IsValid());
  if (!writer_) {
    writer_ = std::make_unique<BufferedFileWriter>(
        store_path_, prefix_size_, buffer_size, extension, metric_prefix);
  }
  return writer_.get();
}

const std::string& HashPrefixContainer::FileInfo::GetExtensionForTesting()
    const {
  return writer_->extension();
}

bool HashPrefixContainer::FileInfo::IsReadable() const {
  return file_.IsValid();
}

base::FilePath HashPrefixContainer::FileInfo::GetPath() const {
  return HashPrefixContainer::GetPath(store_path_, extension_);
}

// static
base::FilePath HashPrefixContainer::GetPath(const base::FilePath& store_path,
                                            const std::string& extension) {
  return store_path.AddExtensionASCII(extension);
}

HashPrefixContainer::HashPrefixContainer(
    const base::FilePath& store_path,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    size_t buffer_size)
    : store_path_(store_path),
      task_runner_(task_runner
                       ? std::move(task_runner)
                       : base::SequencedTaskRunner::GetCurrentDefault()),
      buffer_size_(buffer_size) {}

HashPrefixContainer::~HashPrefixContainer() {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
}

}  // namespace safe_browsing
