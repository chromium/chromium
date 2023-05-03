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

#ifndef CHROME_UTILITY_SAFE_BROWSING_RAR_ANALYZER_H_
#define CHROME_UTILITY_SAFE_BROWSING_RAR_ANALYZER_H_

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/utility/safe_browsing/archive_analyzer.h"
#include "components/safe_browsing/content/common/proto/download_file_types.pb.h"
#include "third_party/unrar/google/unrar_wrapper.h"

namespace safe_browsing {

class RarAnalyzer : public ArchiveAnalyzer {
 public:
  RarAnalyzer();

  ~RarAnalyzer() override;

  RarAnalyzer(const RarAnalyzer&) = delete;
  RarAnalyzer& operator=(const RarAnalyzer&) = delete;

 private:
  void Init() override;
  bool ResumeExtraction() override;
  base::WeakPtr<ArchiveAnalyzer> GetWeakPtr() override;

  void OnGetTempFile(base::File temp_file);

  base::File temp_file_;
  third_party_unrar::RarReader reader_;

  base::WeakPtrFactory<RarAnalyzer> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_UTILITY_SAFE_BROWSING_RAR_ANALYZER_H_
