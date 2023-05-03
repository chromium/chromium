// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_SAFE_BROWSING_DOCUMENT_ANALYZER_H_
#define CHROME_UTILITY_SAFE_BROWSING_DOCUMENT_ANALYZER_H_

#include "base/files/file.h"
#include "base/files/file_path.h"

namespace safe_browsing {
struct DocumentAnalyzerResults;

namespace document_analyzer {
void AnalyzeDocument(base::File office_file,
                     const base::FilePath& file_path,
                     DocumentAnalyzerResults* results);

}  // namespace document_analyzer
}  // namespace safe_browsing

#endif  // CHROME_UTILITY_SAFE_BROWSING_DOCUMENT_ANALYZER_H_
