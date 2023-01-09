// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/public/cpp/sandboxed_document_analyzer.h"

#include <string>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/safe_browsing/document_analyzer_results.h"
#include "chrome/services/file_util/document_analysis_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace {
class SandboxedDocumentAnalyzerTest : public testing::Test {
 public:
  SandboxedDocumentAnalyzerTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  void AnalyzeDocument(const base::FilePath& file_path,
                       safe_browsing::DocumentAnalyzerResults* results) {
    mojo::PendingRemote<chrome::mojom::DocumentAnalysisService> remote;
    DocumentAnalysisService service(remote.InitWithNewPipeAndPassReceiver());

    base::RunLoop run_loop;
    ResultsGetter results_getter(run_loop.QuitClosure(), results);
    scoped_refptr<SandboxedDocumentAnalyzer> analyzer(
        new SandboxedDocumentAnalyzer(file_path, file_path,
                                      results_getter.GetCallback(),
                                      std::move(remote)));
    analyzer->Start();
    run_loop.Run();
  }

  base::FilePath GetFilePath(const char* file_name) {
    base::FilePath test_data;
    EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data));
    return test_data.AppendASCII("safe_browsing")
        .AppendASCII("documents")
        .AppendASCII(file_name);
  }

 private:
  // Helper to provide a SandboxedDocumentAnalyzer::ResultCallback that
  // stores the analysis results and then runs the done closure.
  class ResultsGetter {
    using DocumentAnalyzerResults = safe_browsing::DocumentAnalyzerResults;

   public:
    ResultsGetter(base::OnceClosure done, DocumentAnalyzerResults* results)
        : done_closure_(std::move(done)), results_(results) {}

    SandboxedDocumentAnalyzer::ResultCallback GetCallback() {
      return base::BindOnce(&ResultsGetter::ResultsCallback,
                            base::Unretained(this));
    }

    ResultsGetter(const ResultsGetter&) = delete;
    ResultsGetter& operator=(const ResultsGetter&) = delete;

   private:
    void ResultsCallback(const DocumentAnalyzerResults& results) {
      *results_ = results;
      std::move(done_closure_).Run();
    }

    base::OnceClosure done_closure_;
    raw_ptr<DocumentAnalyzerResults> results_;
  };

  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(SandboxedDocumentAnalyzerTest, AnalyzeDocumentWithMacros) {
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("doc_containing_macros.doc"));
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);

  safe_browsing::DocumentAnalyzerResults results;
  AnalyzeDocument(path, &results);

  EXPECT_TRUE(results.success);
  EXPECT_EQ(ClientDownloadRequest::DocumentProcessingInfo::OK,
            results.error_code);
  EXPECT_TRUE(results.error_message.empty());
  EXPECT_TRUE(results.has_macros);
}

TEST_F(SandboxedDocumentAnalyzerTest, AnalyzeDocumentWithoutMacros) {
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("docx_without_macros.docx"));
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);

  safe_browsing::DocumentAnalyzerResults results;
  AnalyzeDocument(path, &results);

  EXPECT_TRUE(results.success);
  EXPECT_EQ(ClientDownloadRequest::DocumentProcessingInfo::OK,
            results.error_code);
  EXPECT_TRUE(results.error_message.empty());
  EXPECT_FALSE(results.has_macros);
}

TEST_F(SandboxedDocumentAnalyzerTest, AnalyzeUnsupportedFileType) {
  base::FilePath temp_path;
  base::CreateTemporaryFile(&temp_path);
  base::WriteFile(temp_path, "test");
  base::File file(temp_path, base::File::FLAG_OPEN | base::File::FLAG_READ);

  safe_browsing::DocumentAnalyzerResults results;
  AnalyzeDocument(temp_path, &results);

  EXPECT_FALSE(results.success);
  EXPECT_EQ(ClientDownloadRequest::DocumentProcessingInfo::UNSUPPORTED_DOC_TYPE,
            results.error_code);
  EXPECT_FALSE(results.error_message.empty());
  EXPECT_EQ(
      "INTERNAL: Unsupported Type: UNKNOWN_TYPE "
      "[type.googleapis.com/"
      "google.maldoca.MaldocaErrorCode='UNSUPPORTED_DOC_TYPE']",
      results.error_message);
  EXPECT_FALSE(results.has_macros);
}

TEST_F(SandboxedDocumentAnalyzerTest, AnalyzeCorruptedArchive) {
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("txt_as_xlsx.xlsx"));
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);

  safe_browsing::DocumentAnalyzerResults results;
  AnalyzeDocument(path, &results);

  EXPECT_FALSE(results.success);
  EXPECT_EQ(ClientDownloadRequest::DocumentProcessingInfo::ARCHIVE_CORRUPTED,
            results.error_code);
  EXPECT_FALSE(results.error_message.empty());
  EXPECT_EQ(
      "INTERNAL: Unable to open archive! "
      "[type.googleapis.com/"
      "google.maldoca.MaldocaErrorCode='ARCHIVE_CORRUPTED']",
      results.error_message);
  EXPECT_FALSE(results.has_macros);
}
}  // namespace
}  // namespace safe_browsing
