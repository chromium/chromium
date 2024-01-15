// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the 7z file analysis implementation for download
// protection, which runs in a sandboxed utility process.

#ifndef CHROME_UTILITY_SAFE_BROWSING_SEVEN_ZIP_ANALYZER_H_
#define CHROME_UTILITY_SAFE_BROWSING_SEVEN_ZIP_ANALYZER_H_

#include <optional>

#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/functional/callback.h"
#include "chrome/utility/safe_browsing/archive_analyzer.h"
#include "components/safe_browsing/content/common/proto/download_file_types.pb.h"
#include "third_party/lzma_sdk/google/seven_zip_reader.h"

namespace safe_browsing {

class SevenZipAnalyzer : public seven_zip::Delegate, public ArchiveAnalyzer {
 public:
  SevenZipAnalyzer();

  ~SevenZipAnalyzer() override;

  SevenZipAnalyzer(const SevenZipAnalyzer&) = delete;
  SevenZipAnalyzer& operator=(const SevenZipAnalyzer&) = delete;

  // seven_zip::Delegate
  void OnOpenError(seven_zip::Result result) override;
  base::File OnTempFileRequest() override;
  bool OnEntry(const seven_zip::EntryInfo& entry,
               base::span<uint8_t>& output) override;
  bool OnDirectory(const seven_zip::EntryInfo& entry) override;
  bool EntryDone(seven_zip::Result result,
                 const seven_zip::EntryInfo& entry) override;

 private:
  void Init() override;
  bool ResumeExtraction() override;
  base::WeakPtr<ArchiveAnalyzer> GetWeakPtr() override;

  void OnGetTempFile(base::File temp_file);

  base::File temp_file_;
  base::File temp_file2_;
  std::unique_ptr<seven_zip::SevenZipReader> reader_;
  std::optional<base::MemoryMappedFile> mapped_file_;

  bool awaiting_nested_ = false;

  base::WeakPtrFactory<SevenZipAnalyzer> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_UTILITY_SAFE_BROWSING_SEVEN_ZIP_ANALYZER_H_
