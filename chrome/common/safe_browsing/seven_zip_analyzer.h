// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the 7z file analysis implementation for download
// protection, which runs in a sandboxed utility process.

#ifndef CHROME_COMMON_SAFE_BROWSING_SEVEN_ZIP_ANALYZER_H_
#define CHROME_COMMON_SAFE_BROWSING_SEVEN_ZIP_ANALYZER_H_

#include "base/files/file.h"

namespace safe_browsing {

struct ArchiveAnalyzerResults;

namespace seven_zip_analyzer {

void AnalyzeSevenZipFile(base::File seven_zip_file,
                         base::File temp_file,
                         base::File temp_file2,
                         ArchiveAnalyzerResults* results);

}  // namespace seven_zip_analyzer
}  // namespace safe_browsing

#endif  // CHROME_COMMON_SAFE_BROWSING_SEVEN_ZIP_ANALYZER_H_
