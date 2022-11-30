// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_FILE_UTIL_SAFE_DOCUMENT_ANALYZER_H_
#define CHROME_SERVICES_FILE_UTIL_SAFE_DOCUMENT_ANALYZER_H_

#include "chrome/services/file_util/public/mojom/safe_document_analyzer.mojom.h"

namespace base {
class FilePath;
class File;
}  // namespace base

class SafeDocumentAnalyzer : public chrome::mojom::SafeDocumentAnalyzer {
 public:
  SafeDocumentAnalyzer();
  ~SafeDocumentAnalyzer() override;
  SafeDocumentAnalyzer(const SafeDocumentAnalyzer&) = delete;
  SafeDocumentAnalyzer& operator=(const SafeDocumentAnalyzer&) = delete;

 private:
  // chrome::mojom::SafeDocumentAnalyzer:
  void AnalyzeDocument(base::File office_file,
                       const base::FilePath& file_path,
                       AnalyzeDocumentCallback callback) override;
};

#endif  // CHROME_SERVICES_FILE_UTIL_SAFE_DOCUMENT_ANALYZER_H_
