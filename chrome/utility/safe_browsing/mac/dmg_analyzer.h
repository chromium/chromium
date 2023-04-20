// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_SAFE_BROWSING_MAC_DMG_ANALYZER_H_
#define CHROME_UTILITY_SAFE_BROWSING_MAC_DMG_ANALYZER_H_

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/utility/safe_browsing/mac/dmg_iterator.h"
#include "chrome/utility/safe_browsing/mac/read_stream.h"
#include "components/safe_browsing/content/common/file_type_policies.h"

namespace safe_browsing {

class ZipAnalyzer;
class RarAnalyzer;
struct ArchiveAnalyzerResults;

namespace dmg {

using FinishedAnalysisCallback = base::OnceClosure;
using GetTempFileCallback =
    base::RepeatingCallback<void(base::OnceCallback<void(base::File)>)>;

class DMGAnalyzer {
 public:
  DMGAnalyzer();

  virtual ~DMGAnalyzer();

  DMGAnalyzer(const DMGAnalyzer&) = delete;
  DMGAnalyzer& operator=(const DMGAnalyzer&) = delete;

  // Init() prepares the DMGAnalyzer to analyze the contents of `dmg_file`,
  // which will notify the `finished_analysis_callback` when `results` are
  // ready. If nested archives are encountered, `get_temp_file_callback`
  // will be used to create new temporary files to analyze those nested
  // archives. Results from nested archives will report as being below
  // `root_dmg_path`. Note that `root_dmg_path` is not a full filesystem
  // path on-disk and represents the DMG virtual filesystem.
  void Init(base::File dmg_file,
            base::FilePath root_dmg_path,
            FinishedAnalysisCallback finished_analysis_callback,
            GetTempFileCallback get_temp_file_callback,
            ArchiveAnalyzerResults* results);

  // Helper function exposed for testing.
  void AnalyzeDMGFileForTesting(std::unique_ptr<DMGIterator> iterator,
                                ArchiveAnalyzerResults* results,
                                base::File temp_file);

 private:
  // Ensures that the `dmg_file_` and `temp_file` are both valid and should
  // be analyzed. `temp_file` is used should the `DMGAnalyzer` encounter a
  // nested archive. After checking for validity calls AnalyzeDMGFile.
  void FilePreChecks(base::File temp_file);

  // Analyzes the given `dmg_file_` for executable content and places the
  // results in `results_`.
  void AnalyzeDMGFile();

  // Checks the `file_type` and creates a new analyzer if the file is a
  // nested archive. Returns true is a nested archive is getting unpacked
  // false otherwise.
  bool AnalyzeNestedArchive(
      safe_browsing::DownloadFileType_InspectionType file_type,
      base::FilePath path);

  // Called from `nested_dmg_analyzer_` using
  // `finished_analysis_callback_`. If unsuccessful, records unpacked
  // archive in results
  void NestedAnalysisFinished(base::FilePath path);

  // Tracks overall file path while unpacking nested archives.
  base::FilePath root_dmg_path_;

  base::File temp_file_;
  base::File dmg_file_;
  raw_ptr<ArchiveAnalyzerResults> results_;
  FinishedAnalysisCallback finished_analysis_callback_;
  GetTempFileCallback get_temp_file_callback_;
  std::unique_ptr<FileReadStream> read_stream_;
  std::unique_ptr<DMGIterator> iterator_;

  // The below analyzers are used to unpack nested archives using
  // DFS to unpacked nested archives.
  // TODO(crbug.com/1426164) Create a common class to hold all analyzers.
  std::unique_ptr<safe_browsing::RarAnalyzer> nested_rar_analyzer_;
  std::unique_ptr<safe_browsing::ZipAnalyzer> nested_zip_analyzer_;
  std::unique_ptr<DMGAnalyzer> nested_dmg_analyzer_;

  base::WeakPtrFactory<DMGAnalyzer> weak_factory_{this};
};

}  // namespace dmg
}  // namespace safe_browsing

#endif  // CHROME_UTILITY_SAFE_BROWSING_MAC_DMG_ANALYZER_H_
