// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_FILE_UTIL_OBFUSCATED_ARCHIVE_ANALYSIS_DELEGATE_H_
#define CHROME_SERVICES_FILE_UTIL_OBFUSCATED_ARCHIVE_ANALYSIS_DELEGATE_H_

#include "chrome/utility/safe_browsing/archive_analysis_delegate.h"
#include "components/enterprise/obfuscation/core/utils.h"

namespace safe_browsing {

class ObfuscatedArchiveAnalysisDelegate : public ArchiveAnalysisDelegate {
 public:
  explicit ObfuscatedArchiveAnalysisDelegate(
      enterprise_obfuscation::HeaderData header_data);
  ~ObfuscatedArchiveAnalysisDelegate() override;

  // ArchiveAnalysisDelegate:
  std::unique_ptr<zip::ReaderDelegate> CreateZipReaderDelegate(
      base::File file) override;
  std::unique_ptr<SafeBrowsingZipWriterDelegate> CreateZipWriterDelegate(
      base::File file) override;
  std::unique_ptr<ArchiveAnalysisDelegate> CreateNestedDelegate(
      base::File extracted_file) override;

 private:
  enterprise_obfuscation::HeaderData header_data_;
};

}  // namespace safe_browsing

#endif  // CHROME_SERVICES_FILE_UTIL_OBFUSCATED_ARCHIVE_ANALYSIS_DELEGATE_H_
