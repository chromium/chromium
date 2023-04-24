// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the 7z file analysis implementation for download
// protection, which runs in a sandboxed utility process.

#ifndef CHROME_COMMON_SAFE_BROWSING_SEVEN_ZIP_ANALYZER_H_
#define CHROME_COMMON_SAFE_BROWSING_SEVEN_ZIP_ANALYZER_H_

#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/functional/callback.h"
#include "chrome/common/safe_browsing/rar_analyzer.h"
#include "chrome/common/safe_browsing/zip_analyzer.h"
#include "components/safe_browsing/content/common/proto/download_file_types.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/lzma_sdk/google/seven_zip_reader.h"

namespace safe_browsing {

struct ArchiveAnalyzerResults;
using FinishedAnalysisCallback = base::OnceCallback<void()>;
using GetTempFileCallback =
    base::RepeatingCallback<void(base::OnceCallback<void(base::File)>)>;

class SevenZipAnalyzer : public seven_zip::Delegate {
 public:
  SevenZipAnalyzer();

  ~SevenZipAnalyzer() override;

  SevenZipAnalyzer(const SevenZipAnalyzer&) = delete;
  SevenZipAnalyzer& operator=(const SevenZipAnalyzer&) = delete;

  // Loads variables and fetches the needed `temp_file` from the
  // `temp_file_getter`.
  void Init(base::File seven_zip_file,
            base::FilePath seven_zip_path,
            FinishedAnalysisCallback finished_analysis_callback,
            GetTempFileCallback get_temp_file_callback,
            ArchiveAnalyzerResults* results);

  // seven_zip::Delegate
  void OnOpenError(seven_zip::Result result) override;
  base::File OnTempFileRequest() override;
  bool OnEntry(const seven_zip::EntryInfo& entry,
               base::span<uint8_t>& output) override;
  bool OnDirectory(const seven_zip::EntryInfo& entry) override;
  bool EntryDone(seven_zip::Result result,
                 const seven_zip::EntryInfo& entry) override;

 private:
  // Ensures that the `seven_zip_path` and both `temp_file`(s) are both
  // valid and should be analyzed.
  void FilePreChecks(base::File temp_file);

  void AnalyzeSevenZipFile();

  // Checks the `file_type` and creates a new analyzer if the file is a
  // nested archive. Returns true when a new analyzer is created, and
  // false when one is not.
  bool AnalyzeNestedArchive(
      safe_browsing::DownloadFileType_InspectionType file_type,
      base::FilePath path);

  // Called from a nested analyzer using
  // `finished_analysis_callback_`. If unsuccessful, records unpacked
  // archive in results
  void NestedAnalysisFinished(base::FilePath path);

  base::FilePath root_seven_zip_path_;

  base::File seven_zip_file_;
  base::File temp_file_;
  base::File temp_file2_;
  raw_ptr<ArchiveAnalyzerResults> results_;
  std::unique_ptr<seven_zip::SevenZipReader> reader_;
  absl::optional<base::MemoryMappedFile> mapped_file_;

  FinishedAnalysisCallback finished_analysis_callback_;
  GetTempFileCallback get_temp_file_callback_;

  // The below analyzers are used to unpack nested archives using
  // DFS to unpacked nested archives.
  // TODO(crbug.com/1426164) Create a common class to hold all analyzers.
  std::unique_ptr<safe_browsing::RarAnalyzer> nested_rar_analyzer_;
  std::unique_ptr<safe_browsing::ZipAnalyzer> nested_zip_analyzer_;
  std::unique_ptr<safe_browsing::SevenZipAnalyzer> nested_seven_zip_analyzer_;
  bool awaiting_nested_ = false;

  base::WeakPtrFactory<SevenZipAnalyzer> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_COMMON_SAFE_BROWSING_SEVEN_ZIP_ANALYZER_H_
