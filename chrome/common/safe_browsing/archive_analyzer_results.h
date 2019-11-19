// Copyright 2015 The Chromium Authors. All rights reserved.
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
#include "components/safe_browsing/proto/csd.pb.h"

namespace base {
class File;
}

namespace safe_browsing {

struct ArchiveAnalyzerResults {
  bool success;
  bool has_executable;
  bool has_archive;
  google::protobuf::RepeatedPtrField<ClientDownloadRequest_ArchivedBinary>
      archived_binary;
  std::vector<base::FilePath> archived_archive_filenames;
#if defined(OS_MACOSX)
  std::vector<uint8_t> signature_blob;
  google::protobuf::RepeatedPtrField<
      ClientDownloadRequest_DetachedCodeSignature>
      detached_code_signatures;
#endif  // OS_MACOSX
  int file_count;
  int directory_count;
  ArchiveAnalyzerResults();
  ArchiveAnalyzerResults(const ArchiveAnalyzerResults& other);
  ~ArchiveAnalyzerResults();
};

// Updates |results| with the results of inspecting |file|, given that it will
// be extracted to |path|. Due to complications with the utility process sandbox
// (see https://crbug.com/944633), the file inspection is limited to the first
// |file_length| bytes of |file|.
void UpdateArchiveAnalyzerResultsWithFile(base::FilePath path,
                                          base::File* file,
                                          int file_length,
                                          bool is_encrypted,
                                          ArchiveAnalyzerResults* results);

}  // namespace safe_browsing

#endif  // CHROME_COMMON_SAFE_BROWSING_ARCHIVE_ANALYZER_RESULTS_H_
