// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the archive file analysis implementation for download
// protection, which runs in a sandboxed utility process.

#include "chrome/common/safe_browsing/archive_analyzer_results.h"

#include "base/files/file.h"
#include "base/i18n/streaming_utf8_validator.h"
#include "base/memory/scoped_refptr.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/common/safe_browsing/binary_feature_extractor.h"
#include "chrome/common/safe_browsing/download_type_util.h"
#include "chrome/common/safe_browsing/file_type_policies.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"

#if defined(OS_MACOSX)
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include "base/containers/span.h"
#include "chrome/common/safe_browsing/disk_image_type_sniffer_mac.h"
#include "chrome/common/safe_browsing/mach_o_image_reader_mac.h"
#endif  // OS_MACOSX

namespace safe_browsing {

namespace {

void SetLengthAndDigestForContainedFile(
    const base::FilePath& path,
    base::File* temp_file,
    int file_length,
    ClientDownloadRequest::ArchivedBinary* archived_binary) {
  std::string file_basename(path.BaseName().AsUTF8Unsafe());
  if (base::StreamingUtf8Validator::Validate(file_basename))
    archived_binary->set_file_basename(file_basename);
  archived_binary->set_length(file_length);

  std::unique_ptr<crypto::SecureHash> hasher =
      crypto::SecureHash::Create(crypto::SecureHash::SHA256);

  const size_t kReadBufferSize = 4096;
  char block[kReadBufferSize];

  int bytes_read_previously = 0;
  temp_file->Seek(base::File::Whence::FROM_BEGIN, 0);
  while (true) {
    int bytes_read_now = temp_file->ReadAtCurrentPos(block, kReadBufferSize);

    if (bytes_read_previously + bytes_read_now > file_length)
      bytes_read_now = file_length - bytes_read_previously;

    if (bytes_read_now <= 0)
      break;

    hasher->Update(block, bytes_read_now);
    bytes_read_previously += bytes_read_now;
  }

  uint8_t digest[crypto::kSHA256Length];
  hasher->Finish(digest, base::size(digest));
  archived_binary->mutable_digests()->set_sha256(digest, base::size(digest));
}

void AnalyzeContainedBinary(
    const scoped_refptr<BinaryFeatureExtractor>& binary_feature_extractor,
    base::File* temp_file,
    ClientDownloadRequest::ArchivedBinary* archived_binary) {
  if (!binary_feature_extractor->ExtractImageFeaturesFromFile(
          temp_file->Duplicate(), BinaryFeatureExtractor::kDefaultOptions,
          archived_binary->mutable_image_headers(),
          archived_binary->mutable_signature()->mutable_signed_data())) {
    archived_binary->clear_image_headers();
    archived_binary->clear_signature();
  } else if (!archived_binary->signature().signed_data_size()) {
    // No SignedData blobs were extracted, so clear the
    // signature field.
    archived_binary->clear_signature();
  }
}

}  // namespace

ArchiveAnalyzerResults::ArchiveAnalyzerResults()
    : success(false),
      has_executable(false),
      has_archive(false),
      file_count(0),
      directory_count(0) {}

ArchiveAnalyzerResults::ArchiveAnalyzerResults(
    const ArchiveAnalyzerResults& other) = default;

ArchiveAnalyzerResults::~ArchiveAnalyzerResults() {}

void UpdateArchiveAnalyzerResultsWithFile(base::FilePath path,
                                          base::File* file,
                                          int file_length,
                                          bool is_encrypted,
                                          ArchiveAnalyzerResults* results) {
  scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor(
      new BinaryFeatureExtractor());
  bool current_entry_is_executable;

#if defined(OS_MACOSX)
  uint32_t magic;
  file->Read(0, reinterpret_cast<char*>(&magic), sizeof(uint32_t));

  char dmg_header[DiskImageTypeSnifferMac::AppleDiskImageTrailerSize()];
  file->Read(0, dmg_header,
             DiskImageTypeSnifferMac::AppleDiskImageTrailerSize());

  current_entry_is_executable =
      FileTypePolicies::GetInstance()->IsCheckedBinaryFile(path) ||
      MachOImageReader::IsMachOMagicValue(magic) ||
      DiskImageTypeSnifferMac::IsAppleDiskImageTrailer(
          base::span<const uint8_t>(
              reinterpret_cast<const uint8_t*>(dmg_header),
              DiskImageTypeSnifferMac::AppleDiskImageTrailerSize()));

  // We can skip checking the trailer if we already know the file is executable.
  if (!current_entry_is_executable) {
    char trailer[DiskImageTypeSnifferMac::AppleDiskImageTrailerSize()];
    file->Seek(base::File::Whence::FROM_END,
               DiskImageTypeSnifferMac::AppleDiskImageTrailerSize());
    file->ReadAtCurrentPos(
        trailer, DiskImageTypeSnifferMac::AppleDiskImageTrailerSize());
    current_entry_is_executable =
        DiskImageTypeSnifferMac::IsAppleDiskImageTrailer(
            base::span<const uint8_t>(
                reinterpret_cast<const uint8_t*>(trailer),
                DiskImageTypeSnifferMac::AppleDiskImageTrailerSize()));
  }

#else
  current_entry_is_executable =
      FileTypePolicies::GetInstance()->IsCheckedBinaryFile(path);
#endif  // OS_MACOSX

  if (FileTypePolicies::GetInstance()->IsArchiveFile(path)) {
    DVLOG(2) << "Downloaded a zipped archive: " << path.value();
    results->has_archive = true;
    results->archived_archive_filenames.push_back(path.BaseName());
    ClientDownloadRequest::ArchivedBinary* archived_archive =
        results->archived_binary.Add();
    archived_archive->set_download_type(ClientDownloadRequest::ARCHIVE);
    archived_archive->set_is_encrypted(is_encrypted);
    SetLengthAndDigestForContainedFile(path, file, file_length,
                                       archived_archive);
  } else if (current_entry_is_executable) {
#if defined(OS_MACOSX)
    // This check prevents running analysis on .app files since they are
    // really just directories and will cause binary feature extraction
    // to fail.
    if (path.Extension().compare(".app") == 0) {
      DVLOG(2) << "Downloaded a zipped .app directory: " << path.value();
    } else {
#endif  // OS_MACOSX
      DVLOG(2) << "Downloaded a zipped executable: " << path.value();
      results->has_executable = true;
      ClientDownloadRequest::ArchivedBinary* archived_binary =
          results->archived_binary.Add();
      archived_binary->set_is_encrypted(is_encrypted);
      archived_binary->set_download_type(
          download_type_util::GetDownloadType(path));
      SetLengthAndDigestForContainedFile(path, file, file_length,
                                         archived_binary);
      AnalyzeContainedBinary(binary_feature_extractor, file, archived_binary);
#if defined(OS_MACOSX)
    }
#endif  // OS_MACOSX
  } else {
    DVLOG(3) << "Ignoring non-binary file: " << path.value();
  }
}

}  // namespace safe_browsing
