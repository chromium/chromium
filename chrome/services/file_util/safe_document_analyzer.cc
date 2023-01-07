// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/safe_document_analyzer.h"

#include "build/build_config.h"
#include "chrome/common/safe_browsing/document_analyzer.h"
#include "chrome/common/safe_browsing/document_analyzer_results.h"

SafeDocumentAnalyzer::SafeDocumentAnalyzer() = default;

SafeDocumentAnalyzer::~SafeDocumentAnalyzer() = default;

void SafeDocumentAnalyzer::AnalyzeDocument(base::File office_file,
                                           const base::FilePath& file_path,
                                           AnalyzeDocumentCallback callback) {
  DCHECK(!file_path.value().empty());
  DCHECK(office_file.IsValid());

  safe_browsing::DocumentAnalyzerResults results;
  safe_browsing::document_analyzer::AnalyzeDocument(std::move(office_file),
                                                    file_path, &results);
  std::move(callback).Run(results);
}
