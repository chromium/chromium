// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the result type of the document analysis that takes place
// in document_analyzer.cc

#ifndef CHROME_COMMON_SAFE_BROWSING_DOCUMENT_ANALYZER_RESULTS_H_
#define CHROME_COMMON_SAFE_BROWSING_DOCUMENT_ANALYZER_RESULTS_H_

#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

struct DocumentAnalyzerResults {
  bool success{false};
  ClientDownloadRequest::DocumentProcessingInfo::MaldocaErrorType error_code;
  bool has_macros{false};
  std::string error_message;
  DocumentAnalyzerResults();
  DocumentAnalyzerResults(const DocumentAnalyzerResults& other);
  ~DocumentAnalyzerResults();
};

}  // namespace safe_browsing

#endif  // CHROME_COMMON_SAFE_BROWSING_DOCUMENT_ANALYZER_RESULTS_H_
