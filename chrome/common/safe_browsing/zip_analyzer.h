// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the zip file analysis implementation for download
// protection, which runs in a sandboxed utility process.

#ifndef CHROME_COMMON_SAFE_BROWSING_ZIP_ANALYZER_H_
#define CHROME_COMMON_SAFE_BROWSING_ZIP_ANALYZER_H_

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "third_party/zlib/google/zip_reader.h"

namespace safe_browsing {
class RarAnalyzer;

struct ArchiveAnalyzerResults;
using FinishedAnalysisCallback = base::OnceCallback<void()>;
using GetTempFileCallback =
    base::RepeatingCallback<void(base::OnceCallback<void(base::File)>)>;

class ZipAnalyzer {
 public:
  ZipAnalyzer();

  virtual ~ZipAnalyzer();

  ZipAnalyzer(const ZipAnalyzer&) = delete;
  ZipAnalyzer& operator=(const ZipAnalyzer&) = delete;

  // Loads variables and fetches the needed `temp_file` from the
  // `temp_file_getter`.
  void Init(base::File zip_file,
            base::FilePath root_zip_path,
            FinishedAnalysisCallback finished_analysis_callback,
            GetTempFileCallback get_temp_file_callback,
            ArchiveAnalyzerResults* results);

 private:
  // Ensures that the `zip_file` and `temp_file` are both valid and should
  // be analyzed.
  void FilePreChecks(base::File temp_file);

  // Analyzes the `zip_file_`. Creates a `nested_zip_analyzer_` when a
  // nested archive is found. Waits for that archive to be analyzed
  // before continuing.
  void AnalyzeZipFile();

  // Checks the `file_type` and creates a new analyzer if the file is a
  // nested archive. Returns true when a new analyzer is created, and
  // false when one is not.
  bool AnalyzeNestedArchive(
      safe_browsing::DownloadFileType_InspectionType file_type,
      base::FilePath path);

  // Called from `nested_zip_analyzer_` using
  // `finished_analysis_callback_`. If unsuccessful, records unpacked
  // archive in results
  void NestedAnalysisFinished(base::FilePath path);

  bool has_encrypted_ = false;
  bool has_aes_encrypted_ = false;

  // Tracks overall file path while unpacking nested archives.
  base::FilePath root_zip_path_;

  base::File zip_file_;
  base::File temp_file_;
  zip::ZipReader reader_;
  ArchiveAnalyzerResults* results_;

  FinishedAnalysisCallback finished_analysis_callback_;
  GetTempFileCallback get_temp_file_callback_;

  // The below analyzers are used to unpack nested archives using
  // DFS.
  // TODO(crbug.com/1426164) Create a common class to hold all analyzers.
  std::unique_ptr<safe_browsing::ZipAnalyzer> nested_zip_analyzer_;
  std::unique_ptr<safe_browsing::RarAnalyzer> nested_rar_analyzer_;

  base::WeakPtrFactory<ZipAnalyzer> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_COMMON_SAFE_BROWSING_ZIP_ANALYZER_H_
