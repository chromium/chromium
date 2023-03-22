// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the rar file analysis implementation for download
// protection, which runs in a sandbox. The reason for running in a sandbox is
// to isolate the browser and other renderer processes from any vulnerabilities
// that the attacker-controlled download file may try to exploit.
//
// Here's the call flow for inspecting .rar files upon download:
// 1. File is downloaded.
// 2. |CheckClientDownloadRequest::AnalyzeFile()| is called to analyze the Safe
//    Browsing reputation of the downloaded file.
// 3. It calls |CheckClientDownloadRequest::StartExtractRarFeatures()|, which
//    creates an instance of |SandboxedRarAnalyzer|, and calls |Start()|.
// 4. |SandboxedRarAnalyzer::Start()| leads to a mojo call to
//    |SafeArchiveAnalyzer::AnalyzeRarFile()| in a sandbox.
// 5. Finally, |SafeArchiveAnalyzer::AnalyzeRarFile()| calls |AnalyzeRarFile()|
//    defined in this file to actually inspect the file.

#ifndef CHROME_COMMON_SAFE_BROWSING_RAR_ANALYZER_H_
#define CHROME_COMMON_SAFE_BROWSING_RAR_ANALYZER_H_

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "components/safe_browsing/content/common/proto/download_file_types.pb.h"
#include "third_party/unrar/google/unrar_wrapper.h"

namespace safe_browsing {

class ZipAnalyzer;

struct ArchiveAnalyzerResults;
using FinishedAnalysisCallback = base::OnceCallback<void()>;
using GetTempFileCallback =
    base::RepeatingCallback<void(base::OnceCallback<void(base::File)>)>;

class RarAnalyzer {
 public:
  RarAnalyzer();

  virtual ~RarAnalyzer();

  RarAnalyzer(const RarAnalyzer&) = delete;
  RarAnalyzer& operator=(const RarAnalyzer&) = delete;

  // Loads variables and fetches the needed `temp_file` from the
  // `temp_file_getter`.
  void Init(base::File rar_file,
            base::FilePath root_rar_path,
            FinishedAnalysisCallback finished_analysis_callback,
            GetTempFileCallback get_temp_file_callback,
            ArchiveAnalyzerResults* results);

 private:
  // Ensures that the `rar_file` and `temp_file` are both valid and should
  // be analyzed.
  void FilePreChecks(base::File temp_file);

  // Analyzes the `rar_file_`. Creates a `nested_rar_analyzer_` when a
  // nested archive is found. Waits for that archive to be analyzed
  // before continuing.
  void AnalyzeRarFile();

  // Checks the `file_type` and creates a new analyzer if the file is a
  // nested archive. Returns true when a new analyzer is created, and
  // false when one is not.
  bool AnalyzeNestedArchive(
      safe_browsing::DownloadFileType_InspectionType file_type,
      base::FilePath path);

  // Called from `nested_rar_analyzer_` using
  // `finished_analysis_callback_`. If unsuccessful, records unpacked
  // archive in results
  void NestedAnalysisFinished(base::FilePath path);

  bool has_encrypted_ = false;

  // Tracks overall file path while unpacking nested archives.
  base::FilePath root_rar_path_;

  base::File rar_file_;
  base::File temp_file_;
  third_party_unrar::RarReader reader_;
  ArchiveAnalyzerResults* results_;

  FinishedAnalysisCallback finished_analysis_callback_;
  GetTempFileCallback get_temp_file_callback_;

  // The below analyzers are used to unpack nested archives using
  // DFS to unpacked nested archives.
  // TODO(crbug.com/1426164) Create a common class to hold all analyzers.
  std::unique_ptr<safe_browsing::RarAnalyzer> nested_rar_analyzer_;
  std::unique_ptr<safe_browsing::ZipAnalyzer> nested_zip_analyzer_;

  base::WeakPtrFactory<RarAnalyzer> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_COMMON_SAFE_BROWSING_RAR_ANALYZER_H_
