// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the zip file analysis implementation for download
// protection, which runs in a sandboxed utility process.

#ifndef CHROME_UTILITY_SAFE_BROWSING_ZIP_ANALYZER_H_
#define CHROME_UTILITY_SAFE_BROWSING_ZIP_ANALYZER_H_

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/utility/safe_browsing/archive_analyzer.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "third_party/zlib/google/zip_reader.h"

namespace safe_browsing {

class ZipAnalyzer : public ArchiveAnalyzer {
 public:
  ZipAnalyzer();
  ~ZipAnalyzer() override;

  ZipAnalyzer(const ZipAnalyzer&) = delete;
  ZipAnalyzer& operator=(const ZipAnalyzer&) = delete;

 private:
  void Init() override;
  bool ResumeExtraction() override;
  base::WeakPtr<ArchiveAnalyzer> GetWeakPtr() override;

  void OnGetTempFile(base::File temp_file);

  base::File temp_file_;
  zip::ZipReader reader_;

  bool has_encrypted_ = false;
  bool has_aes_encrypted_ = false;
  bool has_disk_error_ = false;

  base::WeakPtrFactory<ZipAnalyzer> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_UTILITY_SAFE_BROWSING_ZIP_ANALYZER_H_
