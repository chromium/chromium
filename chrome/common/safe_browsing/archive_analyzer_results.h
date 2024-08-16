// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the archive analyzer analysis implementation for download
// protection, which runs in a sandboxed utility process.

#ifndef CHROME_COMMON_SAFE_BROWSING_ARCHIVE_ANALYZER_RESULTS_H_
#define CHROME_COMMON_SAFE_BROWSING_ARCHIVE_ANALYZER_RESULTS_H_

#include <vector>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "components/safe_browsing/content/common/proto/download_file_types.pb.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace base {
class File;
}

namespace safe_browsing {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ArchiveAnalysisResult {
  kUnknown = 0,  // kUnknown indicates a case where we don't have a specific
                 // reason, but parsing failed. This bucket will be broken into
                 // more buckets in the future, as we identify more reasons for
                 // analysis failure.
  kUnspecified = 1,  // kUnspecified indicates that the analysis code provided
                     // no reason at all. Logging this value indicates a bug.
  kValid = 2,
  kTooLarge = 3,
  kTimeout = 4,
  kFailedToOpen = 5,
  kFailedToOpenTempFile = 6,
  kDmgNoPartitions = 7,
  kFailedDuringIteration = 8,
  kDiskError = 9,
  kMaxValue = kDiskError,
};

struct EncryptionInfo {
  // True if the metadata is encrypted or there is at least one encrypted entry
  // in the archive.
  bool is_encrypted = false;

  // True if the metadata of the top-level archive is encrypted or at
  // least one the files contained in the top-level archive are encrypted.
  bool is_top_level_encrypted = false;

  enum PasswordStatus {
    kUnknown = 0,
    kKnownIncorrect = 1,
    kKnownCorrect = 2,
    kMaxValue = kKnownCorrect,
  };

  // Set to kKnownCorrect if the archive unpacks correctly with the given
  // password.
  PasswordStatus password_status = kUnknown;
};

struct ArchiveAnalyzerResults {
  bool success = false;
  bool has_executable = false;
  bool has_archive = false;
  google::protobuf::RepeatedPtrField<ClientDownloadRequest_ArchivedBinary>
      archived_binary;
  std::vector<base::FilePath> archived_archive_filenames;
#if BUILDFLAG(IS_MAC)
  std::vector<uint8_t> signature_blob;
  google::protobuf::RepeatedPtrField<
      ClientDownloadRequest_DetachedCodeSignature>
      detached_code_signatures;
#endif  // BUILDFLAG(IS_MAC)
  int file_count = 0;
  int directory_count = 0;
  ArchiveAnalysisResult analysis_result = ArchiveAnalysisResult::kUnspecified;

  // TODO(crbug.com/40923880): Populate this information for RAR archives as
  // well.
  EncryptionInfo encryption_info;

  ArchiveAnalyzerResults();
  ArchiveAnalyzerResults(const ArchiveAnalyzerResults& other);
  ~ArchiveAnalyzerResults();
};

// Updates `results` with the results of inspecting `file`, given that it will
// be extracted to `path`. Due to complications with the utility process sandbox
// (see https://crbug.com/944633), the file inspection is limited to the first
// `file_length` bytes of `file`.
void UpdateArchiveAnalyzerResultsWithFile(base::FilePath path,
                                          base::File* file,
                                          int file_length,
                                          bool is_encrypted,
                                          bool is_directory,
                                          bool contents_valid,
                                          bool is_top_level,
                                          ArchiveAnalyzerResults* results);

// Returns the `DownloadFileType_InspectionType` of the file path.
safe_browsing::DownloadFileType_InspectionType GetFileType(base::FilePath path);

// Update the `archived_binary` with the string value path name.
void SetNameForContainedFile(
    const base::FilePath& path,
    ClientDownloadRequest::ArchivedBinary* archived_binary);

// Update the `archived_binary` with the `file_length` and the
// `mutable_digests` fields
void SetLengthAndDigestForContainedFile(
    base::File* temp_file,
    int file_length,
    ClientDownloadRequest::ArchivedBinary* archived_binary);

}  // namespace safe_browsing

#endif  // CHROME_COMMON_SAFE_BROWSING_ARCHIVE_ANALYZER_RESULTS_H_
