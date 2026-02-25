// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_SAFE_BROWSING_ARCHIVE_ANALYSIS_DELEGATE_H_
#define CHROME_UTILITY_SAFE_BROWSING_ARCHIVE_ANALYSIS_DELEGATE_H_

#include <memory>

#include "base/files/file.h"
#include "chrome/utility/safe_browsing/zip_writer_delegate.h"
#include "third_party/zlib/google/zip_reader.h"

#if USE_UNRAR
#include "third_party/unrar/google/unrar_delegates.h"
#endif

namespace safe_browsing {

// Delegate interface for abstracting archive analysis operations, specifically
// for handling obfuscated archives where custom readers and writers are needed.
class ArchiveAnalysisDelegate {
 public:
  virtual ~ArchiveAnalysisDelegate() = default;

  // Creates a reader delegate for reading the ZIP archive.
  virtual std::unique_ptr<zip::ReaderDelegate> CreateZipReaderDelegate(
      base::File file) = 0;

  // Creates a writer delegate for writing extracted ZIP entries.
  virtual std::unique_ptr<SafeBrowsingZipWriterDelegate>
  CreateZipWriterDelegate(base::File file) = 0;

#if USE_UNRAR
  // Creates a reader delegate for reading the RAR archive.
  virtual std::unique_ptr<third_party_unrar::RarReaderDelegate>
  CreateRarReaderDelegate(base::File file) = 0;

  // Creates a writer delegate for writing extracted RAR entries.
  virtual std::unique_ptr<third_party_unrar::RarWriterDelegate>
  CreateRarWriterDelegate(base::File file) = 0;
#endif

  // Creates a delegate for analyzing a nested archive extracted from the
  // current archive. Returns nullptr if the nested archive cannot be handled
  // (e.g. header parsing fails).
  virtual std::unique_ptr<ArchiveAnalysisDelegate> CreateNestedDelegate(
      base::File extracted_file) = 0;
};

}  // namespace safe_browsing

#endif  // CHROME_UTILITY_SAFE_BROWSING_ARCHIVE_ANALYSIS_DELEGATE_H_
