// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the 7z file analysis implementation for download
// protection, which runs in a sandboxed utility process.

#ifndef CHROME_COMMON_SAFE_BROWSING_SEVEN_ZIP_ANALYZER_H_
#define CHROME_COMMON_SAFE_BROWSING_SEVEN_ZIP_ANALYZER_H_

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "components/safe_browsing/content/common/proto/download_file_types.pb.h"

namespace safe_browsing {

struct ArchiveAnalyzerResults;
using FinishedAnalysisCallback = base::OnceCallback<void()>;
using GetTempFileCallback =
    base::RepeatingCallback<void(base::OnceCallback<void(base::File)>)>;

class SevenZipAnalyzer {
 public:
  SevenZipAnalyzer();

  ~SevenZipAnalyzer();

  SevenZipAnalyzer(const SevenZipAnalyzer&) = delete;
  SevenZipAnalyzer& operator=(const SevenZipAnalyzer&) = delete;

  // Loads variables and fetches the needed `temp_file` from the
  // `temp_file_getter`.
  void Init(base::File seven_zip_file,
            base::FilePath seven_zip_path,
            FinishedAnalysisCallback finished_analysis_callback,
            GetTempFileCallback get_temp_file_callback,
            ArchiveAnalyzerResults* results);

 private:
  // Ensures that the `seven_zip_path` and both `temp_file`(s) are both
  // valid and should be analyzed.
  void FilePreChecks(base::File temp_file);

  void AnalyzeSevenZipFile();

  base::FilePath root_seven_zip_path_;

  base::File seven_zip_file_;
  base::File temp_file_;
  base::File temp_file2_;
  raw_ptr<ArchiveAnalyzerResults> results_;

  FinishedAnalysisCallback finished_analysis_callback_;
  GetTempFileCallback get_temp_file_callback_;

  base::WeakPtrFactory<SevenZipAnalyzer> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_COMMON_SAFE_BROWSING_SEVEN_ZIP_ANALYZER_H_
