// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_SAFE_BROWSING_ARCHIVE_ANALYZER_H_
#define CHROME_UTILITY_SAFE_BROWSING_ARCHIVE_ANALYZER_H_

#include <optional>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "components/safe_browsing/content/common/proto/download_file_types.pb.h"

namespace safe_browsing {

using FinishedAnalysisCallback = base::OnceCallback<void()>;
using GetTempFileCallback =
    base::RepeatingCallback<void(base::OnceCallback<void(base::File)>)>;

// Base class for all the archive analyzers. This handles the common behavior
// such as initialization, recursing into nested archives, and updating the
// `ArchiveAnalyzerResults`.
class ArchiveAnalyzer {
 public:
  // Create an `ArchiveAnalyzer` for the given `file_type`. If `file_type` does
  // not support the analyzer, this function will return `nullptr`.
  static std::unique_ptr<ArchiveAnalyzer> CreateForArchiveType(
      DownloadFileType_InspectionType file_type);

  ArchiveAnalyzer();
  virtual ~ArchiveAnalyzer();

  ArchiveAnalyzer(const ArchiveAnalyzer&) = delete;
  ArchiveAnalyzer& operator=(const ArchiveAnalyzer&) = delete;

  void Analyze(base::File archive_file,
               base::FilePath relative_path,
               const std::optional<std::string>& password,
               FinishedAnalysisCallback finished_analysis_callback,
               GetTempFileCallback get_temp_file_callback,
               ArchiveAnalyzerResults* results);

  void SetResultsForTesting(ArchiveAnalyzerResults* results);
  void SetFinishedCallbackForTesting(FinishedAnalysisCallback callback);

 protected:
  // Called when starting extraction. Subclasses should call `InitComplete` when
  // finished.
  virtual void Init() = 0;

  // Called to resume extraction after completing a nested archive. Returns
  // `true` to indicate that extraction has completed, and `false` otherwise.
  virtual bool ResumeExtraction() = 0;

  virtual base::WeakPtr<ArchiveAnalyzer> GetWeakPtr() = 0;

  // Helper functions to expose analyzer state to subclasses.
  base::File& GetArchiveFile();
  const base::FilePath& GetRootPath() const;
  ArchiveAnalyzerResults* results() { return results_; }
  const std::optional<std::string>& password() const { return password_; }

  // Request a temporary file for use during extraction.
  void GetTempFile(base::OnceCallback<void(base::File)> callback);

  // Updates `results_` with the new entry. Returns `true` when extraction
  // should continue, and `false` when the analyzer should pause for
  // asynchronous work.
  bool UpdateResultsForEntry(base::File entry,
                             base::FilePath path,
                             int file_length,
                             bool is_encrypted,
                             bool is_directory,
                             bool contents_valid);

  // Called by `Init` when initialization is complete. If `result` is not
  // `kValid`, analysis is finished with this result. Otherwise we continue with
  // archive unpacking.
  void InitComplete(ArchiveAnalysisResult result);

  // Called from `nested_analyzer_` using
  // `finished_analysis_callback_`. If unsuccessful, records unpacked
  // archive in results.
  void NestedAnalysisFinished(base::File entry,
                              base::FilePath path,
                              int entry_size);

  // Returns whether we're currently unpacking the top-level archive.
  bool IsTopLevelArchive() const;

 private:
  // Tracks the relative path of the current archive within the overall archive
  // being analyzer. The top-level archive will have an empty path, but nested
  // archives will use the path to that directory.
  base::FilePath root_path_;

  base::File archive_file_;
  raw_ptr<ArchiveAnalyzerResults> results_;
  FinishedAnalysisCallback finished_analysis_callback_;
  GetTempFileCallback get_temp_file_callback_;
  std::optional<std::string> password_;

  std::unique_ptr<safe_browsing::ArchiveAnalyzer> nested_analyzer_;
};

}  // namespace safe_browsing

#endif  // CHROME_UTILITY_SAFE_BROWSING_ARCHIVE_ANALYZER_H_
