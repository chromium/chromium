// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the rar file analysis implementation for download
// protection, which runs in a sandbox. The reason for running in a sandbox is
// to isolate the browser and other renderer processes from any vulnerabilities
// that the attacker-controlled download file may try to exploit.
//
// Here's the call flow for inspecting .rar files upon download:
// 1. File is downloaded.
// 2. |CheckClientDownloadRequest::AnalyzeFile()| is called to analyze the Safe
//    Browsing reputation of the downloaded file.
// 3. It calls |CheckClientDownloadRequest::StartExtractRarFeatures()|, which
//    creates an instance of |SandboxedRarAnalyzer|, and calls |Start()|.
// 4. |SandboxedRarAnalyzer::Start()| leads to a mojo call to
//    |SafeArchiveAnalyzer::AnalyzeRarFile()| in a sandbox.
// 5. Finally, |SafeArchiveAnalyzer::AnalyzeRarFile()| calls |AnalyzeRarFile()|
//    defined in this file to actually inspect the file.

#ifndef CHROME_COMMON_SAFE_BROWSING_RAR_ANALYZER_H_
#define CHROME_COMMON_SAFE_BROWSING_RAR_ANALYZER_H_

#include "base/files/file.h"

namespace safe_browsing {

struct ArchiveAnalyzerResults;

namespace rar_analyzer {

// |rar_file| is a platform-agnostic handle to the file, and |temp_file| is a
// handle for a temporary file the sandbox can write to. Since |AnalyzeRarFile|
// runs inside a sandbox, it isn't allowed to open file handles. So both files
// are opened in |SandboxedRarAnalyzer|, which runs in the browser process, and
// the handles are passed here. The function populates the various fields in
// |results| based on the results of parsing the rar file. If the parsing fails
// for any reason, including crashing the sandbox process, the browser process
// considers the file safe.
void AnalyzeRarFile(base::File rar_file,
                    base::File temp_file,
                    ArchiveAnalyzerResults* results);

}  // namespace rar_analyzer
}  // namespace safe_browsing

#endif  // CHROME_COMMON_SAFE_BROWSING_RAR_ANALYZER_H_
