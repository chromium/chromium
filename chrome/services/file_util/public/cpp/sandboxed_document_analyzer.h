// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_FILE_UTIL_PUBLIC_CPP_SANDBOXED_DOCUMENT_ANALYZER_H_
#define CHROME_SERVICES_FILE_UTIL_PUBLIC_CPP_SANDBOXED_DOCUMENT_ANALYZER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/services/file_util/public/mojom/document_analysis_service.mojom.h"
#include "chrome/services/file_util/public/mojom/safe_document_analyzer.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace safe_browsing {
struct DocumentAnalyzerResults;
}

// This class is used to analyze office documents in a sandbox for download
// protection. The class lives on the UI thread, which is where the result
// callback will be invoked
class SandboxedDocumentAnalyzer {
 public:
  using ResultCallback =
      base::OnceCallback<void(const safe_browsing::DocumentAnalyzerResults&)>;

  // Factory function for creating SandboxedDocumentAnalyzers with the
  // appropriate deleter.
  static std::unique_ptr<SandboxedDocumentAnalyzer, base::OnTaskRunnerDeleter>
  CreateAnalyzer(
      const base::FilePath& target_document_path,
      const base::FilePath& tmp_document_path,
      ResultCallback callback,
      mojo::PendingRemote<chrome::mojom::DocumentAnalysisService> service);

  ~SandboxedDocumentAnalyzer();

  SandboxedDocumentAnalyzer(const SandboxedDocumentAnalyzer&) = delete;
  SandboxedDocumentAnalyzer& operator=(const SandboxedDocumentAnalyzer&) =
      delete;

  // Start the analysis. Must be called on the UI thread.
  void Start();

 private:
  SandboxedDocumentAnalyzer(
      const base::FilePath& target_document_path,
      const base::FilePath& tmp_document_path,
      ResultCallback callback,
      mojo::PendingRemote<chrome::mojom::DocumentAnalysisService> service);

  // If file preparation failed, analysis has failed: report failure.
  void ReportFileFailure(const std::string msg);

  // Starts the utility process and sends it a document analysis request.
  void AnalyzeDocument(base::File file);

  // The response containing the document analysis results.
  void AnalyzeDocumentDone(
      const safe_browsing::DocumentAnalyzerResults& results);

  // Returns a weak pointer to this.
  base::WeakPtr<SandboxedDocumentAnalyzer> GetWeakPtr();

  // The target file path of the document once the download is completed.
  base::FilePath target_file_path_;

  // The file path containing the contents of the document to analyze.
  base::FilePath tmp_file_path_;

  // Callback invoked on the UI thread with the document analysis results.
  ResultCallback callback_;

  // Remote interfaces to the document analysis service. Only used from the UI
  // thread.
  mojo::Remote<chrome::mojom::DocumentAnalysisService> service_;
  mojo::Remote<chrome::mojom::SafeDocumentAnalyzer> remote_analyzer_;

  base::WeakPtrFactory<SandboxedDocumentAnalyzer> weak_ptr_factory_{this};
};

#endif  // CHROME_SERVICES_FILE_UTIL_PUBLIC_CPP_SANDBOXED_DOCUMENT_ANALYZER_H_
