// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the archive file analysis implementation for download
// protection, which runs in a sandboxed utility process.

#include "chrome/common/safe_browsing/archive_analyzer_results.h"

#include "base/files/file.h"
#include "base/i18n/streaming_utf8_validator.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/common/safe_browsing/binary_feature_extractor.h"
#include "chrome/common/safe_browsing/download_type_util.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC)
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include "base/containers/span.h"
#include "chrome/common/safe_browsing/disk_image_type_sniffer_mac.h"
#include "chrome/common/safe_browsing/mach_o_image_reader_mac.h"
#endif  // BUILDFLAG(IS_MAC)

namespace safe_browsing {

namespace {

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

ArchiveAnalyzerResults::ArchiveAnalyzerResults() = default;
ArchiveAnalyzerResults::ArchiveAnalyzerResults(
    const ArchiveAnalyzerResults& other) = default;

ArchiveAnalyzerResults::~ArchiveAnalyzerResults() {}

void UpdateArchiveAnalyzerResultsWithFile(base::FilePath path,
                                          base::File* file,
                                          int file_length,
                                          bool is_encrypted,
                                          bool is_directory,
                                          bool contents_valid,
                                          bool is_top_level,
                                          ArchiveAnalyzerResults* results) {
  results->encryption_info.is_encrypted |= is_encrypted;
  if (is_top_level) {
    results->encryption_info.is_top_level_encrypted |= is_encrypted;
  }

  scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor(
      new BinaryFeatureExtractor());
  bool current_entry_is_executable;
#if BUILDFLAG(IS_MAC)
  uint32_t magic;
  file->Read(0, base::byte_span_from_ref(magic));

  uint8_t dmg_header[DiskImageTypeSnifferMac::kAppleDiskImageTrailerSize];
  file->Read(0, dmg_header);

  bool is_checked =
      FileTypePolicies::GetInstance()->IsCheckedBinaryFile(path) &&
      !is_directory;
  current_entry_is_executable =
      is_checked || MachOImageReader::IsMachOMagicValue(magic) ||
      DiskImageTypeSnifferMac::IsAppleDiskImageTrailer(dmg_header);

  // We can skip checking the trailer if we already know the file is executable.
  if (!current_entry_is_executable) {
    uint8_t trailer[DiskImageTypeSnifferMac::kAppleDiskImageTrailerSize];
    file->Seek(base::File::Whence::FROM_END,
               DiskImageTypeSnifferMac::kAppleDiskImageTrailerSize);
    file->ReadAtCurrentPos(trailer);
    current_entry_is_executable =
        DiskImageTypeSnifferMac::IsAppleDiskImageTrailer(trailer);
  }

#else
  current_entry_is_executable =
      FileTypePolicies::GetInstance()->IsCheckedBinaryFile(path) &&
      !is_directory;
#endif  // BUILDFLAG(IS_MAC)

  if (FileTypePolicies::GetInstance()->IsArchiveFile(path)) {
    DVLOG(2) << "Downloaded a zipped archive: " << path.value();
    results->has_archive = true;
    results->archived_archive_filenames.push_back(path.BaseName());
    ClientDownloadRequest::ArchivedBinary* archived_archive =
        results->archived_binary.Add();
    archived_archive->set_download_type(ClientDownloadRequest::ARCHIVE);
    archived_archive->set_is_encrypted(is_encrypted);
    archived_archive->set_is_archive(true);
    SetNameForContainedFile(path, archived_archive);
    if (contents_valid) {
      SetLengthAndDigestForContainedFile(file, file_length, archived_archive);
    }
  } else {
#if BUILDFLAG(IS_MAC)
    // This check prevents running analysis on .app files since they are
    // really just directories and will cause binary feature extraction
    // to fail.
    if (path.Extension().compare(".app") == 0) {
      DVLOG(2) << "Downloaded a zipped .app directory: " << path.value();
    } else {
#endif  // BUILDFLAG(IS_MAC)
      DVLOG(2) << "Downloaded a zipped executable: " << path.value();
      results->has_executable |= current_entry_is_executable;
      ClientDownloadRequest::ArchivedBinary* archived_binary =
          results->archived_binary.Add();
      archived_binary->set_is_encrypted(is_encrypted);
      archived_binary->set_download_type(
          download_type_util::GetDownloadType(path));
      archived_binary->set_is_executable(current_entry_is_executable);
      SetNameForContainedFile(path, archived_binary);
      if (contents_valid) {
        SetLengthAndDigestForContainedFile(file, file_length, archived_binary);
      }
      if (current_entry_is_executable) {
        AnalyzeContainedBinary(binary_feature_extractor, file, archived_binary);
      }
#if BUILDFLAG(IS_MAC)
    }
#endif  // BUILDFLAG(IS_MAC)
  }
}

safe_browsing::DownloadFileType_InspectionType GetFileType(
    base::FilePath path) {
  return FileTypePolicies::GetInstance()
      ->PolicyForFile(path, GURL{}, nullptr)
      .inspection_type();
}

void SetNameForContainedFile(
    const base::FilePath& path,
    ClientDownloadRequest::ArchivedBinary* archived_binary) {
  std::string file_path(path.AsUTF8Unsafe());
  if (base::StreamingUtf8Validator::Validate(file_path)) {
    archived_binary->set_file_path(file_path);
  }
}

void SetLengthAndDigestForContainedFile(
    base::File* temp_file,
    int file_length,
    ClientDownloadRequest::ArchivedBinary* archived_binary) {
  archived_binary->set_length(file_length);

  std::unique_ptr<crypto::SecureHash> hasher =
      crypto::SecureHash::Create(crypto::SecureHash::SHA256);

  const size_t kReadBufferSize = 4096;
  uint8_t block[kReadBufferSize];

  size_t bytes_read_previously = 0;
  temp_file->Seek(base::File::Whence::FROM_BEGIN, 0);
  while (true) {
    std::optional<size_t> bytes_read_now =
        temp_file->ReadAtCurrentPos(base::span(block));

    if (!bytes_read_now) {
      break;
    }

    if (*bytes_read_now > file_length - bytes_read_previously) {
      bytes_read_now = file_length - bytes_read_previously;
    }

    if (*bytes_read_now <= 0) {
      break;
    }

    hasher->Update(base::make_span(block).first(*bytes_read_now));
    bytes_read_previously += *bytes_read_now;
  }

  uint8_t digest[crypto::kSHA256Length];
  hasher->Finish(digest, std::size(digest));
  archived_binary->mutable_digests()->set_sha256(digest, std::size(digest));
}

}  // namespace safe_browsing
