// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_FILE_UTIL_PUBLIC_CPP_SANDBOXED_SEVEN_ZIP_ANALYZER_H_
#define CHROME_SERVICES_FILE_UTIL_PUBLIC_CPP_SANDBOXED_SEVEN_ZIP_ANALYZER_H_

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/services/file_util/public/cpp/temporary_file_getter.h"
#include "chrome/services/file_util/public/mojom/file_util_service.mojom.h"
#include "chrome/services/file_util/public/mojom/safe_archive_analyzer.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace safe_browsing {
enum class ArchiveAnalysisResult;
struct ArchiveAnalyzerResults;
}  // namespace safe_browsing

// This class is used to analyze 7z files in a sandboxed utility process for
// file download protection. This class lives on the UI thread, which is where
// the result callback will be invoked.
class SandboxedSevenZipAnalyzer {
 public:
  using ResultCallback =
      base::OnceCallback<void(const safe_browsing::ArchiveAnalyzerResults&)>;
  using WrappedFilePtr = std::unique_ptr<base::File, base::OnTaskRunnerDeleter>;

  // Factory function for creating SandboxedSevenZipAnalyzers with the
  // appropriate deleter.
  static std::unique_ptr<SandboxedSevenZipAnalyzer, base::OnTaskRunnerDeleter>
  CreateAnalyzer(const base::FilePath& seven_zip_file,
                 ResultCallback callback,
                 mojo::PendingRemote<chrome::mojom::FileUtilService> service);

  ~SandboxedSevenZipAnalyzer();

  SandboxedSevenZipAnalyzer(const SandboxedSevenZipAnalyzer&) = delete;
  SandboxedSevenZipAnalyzer& operator=(const SandboxedSevenZipAnalyzer&) =
      delete;

  // Starts the analysis. Must be called on the UI thread.
  void Start();

 private:
  SandboxedSevenZipAnalyzer(
      const base::FilePath& seven_zip_file,
      ResultCallback callback,
      mojo::PendingRemote<chrome::mojom::FileUtilService> service);

  // If file preparation failed, analysis has failed: report failure.
  void ReportFileFailure(safe_browsing::ArchiveAnalysisResult reason);

  // Starts the utility process and sends it a file analyze request.
  void AnalyzeFile(WrappedFilePtr file);

  // The response containing the file analyze results.
  void AnalyzeFileDone(const safe_browsing::ArchiveAnalyzerResults& results);

  // Returns a weak pointer to this.
  base::WeakPtr<SandboxedSevenZipAnalyzer> GetWeakPtr();

  // The file path of the file to analyze.
  const base::FilePath file_path_;

  // Callback invoked on the UI thread with the file analyze results.
  ResultCallback callback_;

  // Remote interfaces to the file util service. Only used from the UI thread.
  mojo::Remote<chrome::mojom::FileUtilService> service_;
  mojo::Remote<chrome::mojom::SafeArchiveAnalyzer> remote_analyzer_;
  TemporaryFileGetter temp_file_getter_;

  // Task runner for blocking file operations
  const scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  base::WeakPtrFactory<SandboxedSevenZipAnalyzer> weak_ptr_factory_{this};
};

#endif  // CHROME_SERVICES_FILE_UTIL_PUBLIC_CPP_SANDBOXED_SEVEN_ZIP_ANALYZER_H_
