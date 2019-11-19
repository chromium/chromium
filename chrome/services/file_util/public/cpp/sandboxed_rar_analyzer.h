// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_FILE_UTIL_PUBLIC_CPP_SANDBOXED_RAR_ANALYZER_H_
#define CHROME_SERVICES_FILE_UTIL_PUBLIC_CPP_SANDBOXED_RAR_ANALYZER_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/services/file_util/public/mojom/file_util_service.mojom.h"
#include "chrome/services/file_util/public/mojom/safe_archive_analyzer.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace safe_browsing {
struct ArchiveAnalyzerResults;
}

// This class is used to analyze rar files in a sandbox for file download
// protection. This class lives on the UI thread, which is where the result
// callback will be invoked.
class SandboxedRarAnalyzer
    : public base::RefCountedThreadSafe<SandboxedRarAnalyzer> {
 public:
  using ResultCallback = base::RepeatingCallback<void(
      const safe_browsing::ArchiveAnalyzerResults&)>;

  SandboxedRarAnalyzer(
      const base::FilePath& rar_file_path,
      const ResultCallback& callback,
      mojo::PendingRemote<chrome::mojom::FileUtilService> service);

  // Starts the analysis. Must be called on the UI thread.
  void Start();

  // Returns a string reflecting internal state. Useful for debugging.
  std::string DebugString() const;

 private:
  friend class base::RefCountedThreadSafe<SandboxedRarAnalyzer>;

  ~SandboxedRarAnalyzer();

  // Prepare the file for analysis.
  void PrepareFileToAnalyze();

  // If file preparation failed, analysis has failed: report failure.
  void ReportFileFailure();

  // Starts the utility process and sends it a request to analyze the file
  // |file|, given a handle for |temp_file|, where it can extract files.
  void AnalyzeFile(base::File file, base::File temp_file);

  // The response containing the file analyze results.
  void AnalyzeFileDone(const safe_browsing::ArchiveAnalyzerResults& results);

  // The file path of the file to analyze.
  const base::FilePath file_path_;

  // Callback invoked on the UI thread with the file analyze results.
  const ResultCallback callback_;

  // Remote interfaces to the file util service. Only used from the UI thread.
  mojo::Remote<chrome::mojom::FileUtilService> service_;
  mojo::Remote<chrome::mojom::SafeArchiveAnalyzer> remote_analyzer_;

  DISALLOW_COPY_AND_ASSIGN(SandboxedRarAnalyzer);
};

std::ostream& operator<<(std::ostream& os,
                         const SandboxedRarAnalyzer& rar_analyzer);

#endif  // CHROME_SERVICES_FILE_UTIL_PUBLIC_CPP_SANDBOXED_RAR_ANALYZER_H_
