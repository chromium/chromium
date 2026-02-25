// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_FILE_UTIL_REGULAR_ARCHIVE_ANALYSIS_DELEGATE_H_
#define CHROME_SERVICES_FILE_UTIL_REGULAR_ARCHIVE_ANALYSIS_DELEGATE_H_

#include "chrome/utility/safe_browsing/archive_analysis_delegate.h"

#if USE_UNRAR
#include "third_party/unrar/google/unrar_delegates.h"
#endif

namespace safe_browsing {

class RegularArchiveAnalysisDelegate : public ArchiveAnalysisDelegate {
 public:
  RegularArchiveAnalysisDelegate();
  ~RegularArchiveAnalysisDelegate() override;

  // ArchiveAnalysisDelegate:
  std::unique_ptr<zip::ReaderDelegate> CreateZipReaderDelegate(
      base::File file) override;
  std::unique_ptr<SafeBrowsingZipWriterDelegate> CreateZipWriterDelegate(
      base::File file) override;
#if USE_UNRAR
  std::unique_ptr<third_party_unrar::RarReaderDelegate> CreateRarReaderDelegate(
      base::File file) override;
  std::unique_ptr<third_party_unrar::RarWriterDelegate> CreateRarWriterDelegate(
      base::File file) override;
#endif
  std::unique_ptr<ArchiveAnalysisDelegate> CreateNestedDelegate(
      base::File extracted_file) override;
};

}  // namespace safe_browsing

#endif  // CHROME_SERVICES_FILE_UTIL_REGULAR_ARCHIVE_ANALYSIS_DELEGATE_H_
