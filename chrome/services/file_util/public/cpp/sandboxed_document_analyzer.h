// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_FILE_UTIL_PUBLIC_CPP_SANDBOXED_DOCUMENT_ANALYZER_H_
#define CHROME_SERVICES_FILE_UTIL_PUBLIC_CPP_SANDBOXED_DOCUMENT_ANALYZER_H_

#include <string>
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
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

class SandboxedDocumentAnalyzer
    : public base::RefCountedDeleteOnSequence<SandboxedDocumentAnalyzer> {
 public:
  using ResultCallback =
      base::OnceCallback<void(const safe_browsing::DocumentAnalyzerResults&)>;

  SandboxedDocumentAnalyzer(
      const base::FilePath& target_document_path,
      const base::FilePath& tmp_document_path,
      ResultCallback callback,
      mojo::PendingRemote<chrome::mojom::DocumentAnalysisService> service);

  SandboxedDocumentAnalyzer(const SandboxedDocumentAnalyzer&) = delete;
  SandboxedDocumentAnalyzer& operator=(const SandboxedDocumentAnalyzer&) =
      delete;

  // Start the analysis. Must be called on the UI thread.
  void Start();

 private:
  friend class base::RefCountedDeleteOnSequence<SandboxedDocumentAnalyzer>;
  friend class base::DeleteHelper<SandboxedDocumentAnalyzer>;

  ~SandboxedDocumentAnalyzer();

  // If file preparation failed, analysis has failed: report failure.
  void ReportFileFailure(const std::string msg);

  // Prepare the file for analysis.
  void PrepareFileToAnalyze();

  // Starts the utility process and sends it a document analysis request.
  void AnalyzeDocument(base::File file, const base::FilePath& file_path);

  // The response containing the document analysis results.
  void AnalyzeDocumentDone(
      const safe_browsing::DocumentAnalyzerResults& results);

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
};

#endif  // CHROME_SERVICES_FILE_UTIL_PUBLIC_CPP_SANDBOXED_DOCUMENT_ANALYZER_H_
