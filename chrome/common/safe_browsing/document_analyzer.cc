// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the document analyzer analysis implementation for download
// protection, which runs in a sandboxed utility process.

#include "chrome/common/safe_browsing/document_analyzer.h"

#include <string>

#include "base/files/file_path.h"
#include "chrome/common/safe_browsing/document_analyzer_results.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "third_party/maldoca/src/maldoca/service/common/process_doc_wrapper.h"

namespace safe_browsing {
namespace document_analyzer {

void AnalyzeDocument(base::File office_file,
                     const base::FilePath& file_path,
                     DocumentAnalyzerResults* results) {
  results->has_macros = false;
  results->success = false;
  results->error_code = ClientDownloadRequest::DocumentProcessingInfo::UNKNOWN;
  results->error_message = std::string();

  std::string str_error_code;

  third_party_maldoca::AnalyzeOfficeDocument(
      std::move(office_file), file_path, results->has_macros, results->success,
      str_error_code, results->error_message);

  if (!ClientDownloadRequest::DocumentProcessingInfo::MaldocaErrorType_Parse(
          std::move(str_error_code), &results->error_code)) {
    results->error_code =
        ClientDownloadRequest::DocumentProcessingInfo::UNKNOWN;
  }
}
}  // namespace document_analyzer
}  // namespace safe_browsing
