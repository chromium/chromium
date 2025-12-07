// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/obfuscated_archive_analysis_delegate.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "chrome/utility/safe_browsing/zip_writer_delegate.h"
#include "components/enterprise/obfuscation/core/download_obfuscator.h"
#include "components/enterprise/obfuscation/core/obfuscated_file_reader.h"
#include "components/enterprise/obfuscation/core/utils.h"
#include "third_party/zlib/google/zip_writer.h"

namespace safe_browsing {

namespace {

// This is running in a sandboxed utility process. Used to emulate disk read's
// over obfuscated files.
class ObfuscatedZipReaderDelegate : public zip::ReaderDelegate {
 public:
  explicit ObfuscatedZipReaderDelegate(
      enterprise_obfuscation::ObfuscatedFileReader file_reader)
      : file_reader_(std::move(file_reader)) {}
  ~ObfuscatedZipReaderDelegate() override = default;

  // zip::ReaderDelegate:
  int64_t ReadBytes(base::span<uint8_t> data) override {
    return file_reader_.Read(data);
  }

  bool Seek(int64_t offset) override {
    return file_reader_.Seek(offset, base::File::FROM_BEGIN) == offset;
  }

  int64_t Tell() override { return file_reader_.Tell(); }

  int64_t GetLength() override { return file_reader_.GetSize(); }

 private:
  enterprise_obfuscation::ObfuscatedFileReader file_reader_;
};

class ObfuscatedZipWriterDelegate : public zip::FileWriterDelegate,
                                    public SafeBrowsingZipWriterDelegate {
 public:
  explicit ObfuscatedZipWriterDelegate(base::File file)
      : zip::FileWriterDelegate(std::move(file)) {}

  ObfuscatedZipWriterDelegate(const ObfuscatedZipWriterDelegate&) = delete;
  ObfuscatedZipWriterDelegate& operator=(const ObfuscatedZipWriterDelegate&) =
      delete;

  ~ObfuscatedZipWriterDelegate() override { Close(); }

  // zip::FileWriterDelegate overrides:
  bool PrepareOutput() override {
    bool success = zip::FileWriterDelegate::PrepareOutput();
    has_disk_error_ |= !success;
    return success;
  }

  bool WriteBytes(const char* data, int num_bytes) override {
    // SAFETY: zlib guarantees `data` points to at least `num_bytes` valid
    // bytes.
    auto bytes = base::as_bytes(base::span(UNSAFE_BUFFERS(
        base::span(data, base::checked_cast<size_t>(num_bytes)))));
    auto result = obfuscator_.ObfuscateChunk(bytes,
                                             /*is_last_chunk=*/false);

    if (!result.has_value()) {
      has_disk_error_ = true;
      return false;
    }

    auto buffer = base::as_chars(base::span(*result));
    bool success = zip::FileWriterDelegate::WriteBytes(buffer.data(),
                                                       buffer.size());

    if (!success) {
      has_disk_error_ = true;
      return false;
    }
    return true;
  }

  // SafeBrowsingZipWriterDelegate overrides:
  bool has_disk_error() const override { return has_disk_error_; }

  int64_t file_length() const override {
    return zip::FileWriterDelegate::file_length_;
  }

  void Close() override {
    if (!closed_ && !has_disk_error_) {
      auto result = obfuscator_.ObfuscateChunk({}, /*is_last_chunk=*/true);
      has_disk_error_ |= !result.has_value();
      if (result.has_value()) {
        auto buffer = base::as_chars(base::span(result.value()));
        bool success = zip::FileWriterDelegate::WriteBytes(buffer.data(),
                                                           buffer.size_bytes());
        has_disk_error_ |= !success;
      }
    }
    closed_ = true;
  }

 private:
  enterprise_obfuscation::DownloadObfuscator obfuscator_;
  bool has_disk_error_ = false;
  bool closed_ = false;
};

}  // namespace

ObfuscatedArchiveAnalysisDelegate::ObfuscatedArchiveAnalysisDelegate(
    enterprise_obfuscation::HeaderData header_data)
    : header_data_(std::move(header_data)) {}

ObfuscatedArchiveAnalysisDelegate::~ObfuscatedArchiveAnalysisDelegate() =
    default;

std::unique_ptr<zip::ReaderDelegate>
ObfuscatedArchiveAnalysisDelegate::CreateZipReaderDelegate(base::File file) {
  base::expected<enterprise_obfuscation::ObfuscatedFileReader,
                 enterprise_obfuscation::Error>
      file_reader = enterprise_obfuscation::ObfuscatedFileReader::Create(
          header_data_, std::move(file));
  if (!file_reader.has_value()) {
    return nullptr;
  }
  return std::make_unique<ObfuscatedZipReaderDelegate>(
      std::move(file_reader.value()));
}

std::unique_ptr<SafeBrowsingZipWriterDelegate>
ObfuscatedArchiveAnalysisDelegate::CreateZipWriterDelegate(base::File file) {
  return std::make_unique<ObfuscatedZipWriterDelegate>(std::move(file));
}

std::unique_ptr<ArchiveAnalysisDelegate>
ObfuscatedArchiveAnalysisDelegate::CreateNestedDelegate(
    base::File extracted_file) {
  auto header_result =
      enterprise_obfuscation::ObfuscatedFileReader::ReadHeaderData(
          extracted_file);
  if (!header_result.has_value()) {
    return nullptr;
  }
  return std::make_unique<ObfuscatedArchiveAnalysisDelegate>(
      std::move(header_result.value()));
}

}  // namespace safe_browsing
