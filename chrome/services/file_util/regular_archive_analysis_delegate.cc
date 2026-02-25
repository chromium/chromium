// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/regular_archive_analysis_delegate.h"

#include <optional>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "chrome/utility/safe_browsing/zip_writer_delegate.h"
#include "third_party/zlib/google/zip_reader.h"

#if USE_UNRAR
#include "third_party/unrar/google/unrar_delegates.h"
#include "third_party/unrar/google/unrar_wrapper.h"
#endif

namespace safe_browsing {

namespace {

class FileZipReaderDelegate : public zip::ReaderDelegate {
 public:
  explicit FileZipReaderDelegate(base::File file) : file_(std::move(file)) {}
  ~FileZipReaderDelegate() override = default;

  // zip::ReaderDelegate:
  int64_t ReadBytes(base::span<uint8_t> data) override {
    std::optional<size_t> bytes_read = file_.ReadAtCurrentPos(data);
    return bytes_read.has_value() ? static_cast<int64_t>(*bytes_read) : -1;
  }

  bool Seek(int64_t offset) override {
    return file_.Seek(base::File::FROM_BEGIN, offset) == offset;
  }

  int64_t Tell() override {
    return file_.GetLength() < 0 ? -1 : file_.Seek(base::File::FROM_CURRENT, 0);
  }

  int64_t GetLength() override { return file_.GetLength(); }

 private:
  base::File file_;
};

class ZipWriterDelegate : public zip::FileWriterDelegate,
                          public SafeBrowsingZipWriterDelegate {
 public:
  explicit ZipWriterDelegate(base::File file)
      : zip::FileWriterDelegate(std::move(file)) {}
  ZipWriterDelegate(const ZipWriterDelegate&) = delete;
  ZipWriterDelegate& operator=(const ZipWriterDelegate&) = delete;

  ~ZipWriterDelegate() override = default;

  // zip::FileWriterDelegate overrides:
  bool PrepareOutput() override {
    bool success = zip::FileWriterDelegate::PrepareOutput();
    has_disk_error_ |= !success;
    return success;
  }

  bool WriteBytes(base::span<const uint8_t> data) override {
    const bool success = zip::FileWriterDelegate::WriteBytes(data);
    has_disk_error_ |= !success;
    return success;
  }

  // SafeBrowsingZipWriterDelegate overrides:
  bool has_disk_error() const override { return has_disk_error_; }

  int64_t file_length() const override {
    return zip::FileWriterDelegate::file_length_;
  }

 private:
  bool has_disk_error_ = false;
};

}  // namespace

RegularArchiveAnalysisDelegate::RegularArchiveAnalysisDelegate() = default;

RegularArchiveAnalysisDelegate::~RegularArchiveAnalysisDelegate() = default;

std::unique_ptr<zip::ReaderDelegate>
RegularArchiveAnalysisDelegate::CreateZipReaderDelegate(base::File file) {
  return std::make_unique<FileZipReaderDelegate>(std::move(file));
}

std::unique_ptr<SafeBrowsingZipWriterDelegate>
RegularArchiveAnalysisDelegate::CreateZipWriterDelegate(base::File file) {
  return std::make_unique<ZipWriterDelegate>(std::move(file));
}

#if USE_UNRAR
std::unique_ptr<third_party_unrar::RarReaderDelegate>
RegularArchiveAnalysisDelegate::CreateRarReaderDelegate(base::File file) {
  return std::make_unique<third_party_unrar::FileReader>(std::move(file));
}

std::unique_ptr<third_party_unrar::RarWriterDelegate>
RegularArchiveAnalysisDelegate::CreateRarWriterDelegate(base::File file) {
  return std::make_unique<third_party_unrar::FileWriter>(std::move(file));
}
#endif

std::unique_ptr<ArchiveAnalysisDelegate>
RegularArchiveAnalysisDelegate::CreateNestedDelegate(
    base::File extracted_file) {
  return std::make_unique<RegularArchiveAnalysisDelegate>();
}

}  // namespace safe_browsing
