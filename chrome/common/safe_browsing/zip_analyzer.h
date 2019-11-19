// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the zip file analysis implementation for download
// protection, which runs in a sandboxed utility process.

#ifndef CHROME_COMMON_SAFE_BROWSING_ZIP_ANALYZER_H_
#define CHROME_COMMON_SAFE_BROWSING_ZIP_ANALYZER_H_

#include "base/files/file.h"

namespace safe_browsing {

struct ArchiveAnalyzerResults;

namespace zip_analyzer {

void AnalyzeZipFile(base::File zip_file,
                    base::File temp_file,
                    ArchiveAnalyzerResults* results);

}  // namespace zip_analyzer
}  // namespace safe_browsing

#endif  // CHROME_COMMON_SAFE_BROWSING_ZIP_ANALYZER_H_
