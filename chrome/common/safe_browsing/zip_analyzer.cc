// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/zip_analyzer.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/checked_math.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "third_party/zlib/google/zip_reader.h"

namespace safe_browsing {
namespace zip_analyzer {

namespace {

// The maximum duration of ZIP analysis, in milliseconds.
const int kZipAnalysisTimeoutMs = 10000;

}  // namespace

void AnalyzeZipFile(base::File zip_file,
                    base::File temp_file,
                    ArchiveAnalyzerResults* results) {
  base::Time start_time = base::Time::Now();
  zip::ZipReader reader;
  if (!reader.OpenFromPlatformFile(zip_file.GetPlatformFile())) {
    DVLOG(1) << "Failed to open zip file";
    results->analysis_result = ArchiveAnalysisResult::kUnknown;
    return;
  }

  bool too_big_to_unpack =
      base::checked_cast<uint64_t>(zip_file.GetLength()) >
      FileTypePolicies::GetInstance()->GetMaxFileSizeToAnalyze("zip");
  if (too_big_to_unpack) {
    results->success = false;
    results->analysis_result = ArchiveAnalysisResult::kTooLarge;
    return;
  }

  bool timeout = false;
  results->file_count = 0;
  results->directory_count = 0;

  bool has_encrypted = false;
  bool has_aes_encrypted = false;
  while (const zip::ZipReader::Entry* const entry = reader.Next()) {
    if (base::Time::Now() - start_time >
        base::Milliseconds(kZipAnalysisTimeoutMs)) {
      timeout = true;
      break;
    }

    // Clear the |temp_file| between extractions.
    if (temp_file.Seek(base::File::Whence::FROM_BEGIN, 0) != 0)
      PLOG(WARNING) << "Failed seek";

    // Since this code is expected to run within a utility process, this call
    // will fail on some platforms. We handle this by passing the length
    // into `UpdateArchiveAnalyzerResultsWithFile`, which will only consider the
    // appropriate bytes. See crbug.com/1309879 and crbug.com/774762.
    if (!temp_file.SetLength(0))
      PLOG(WARNING) << "Failed truncate";
    zip::FileWriterDelegate writer(&temp_file);
    reader.ExtractCurrentEntry(&writer, std::numeric_limits<uint64_t>::max());
    UpdateArchiveAnalyzerResultsWithFile(entry->path, &temp_file,
                                         writer.file_length(),
                                         entry->is_encrypted, results);

    if (entry->is_directory)
      results->directory_count++;
    else
      results->file_count++;

    has_encrypted |= entry->is_encrypted;
    has_aes_encrypted |= entry->uses_aes_encryption;
  }

  if (has_encrypted) {
    base::UmaHistogramBoolean("SBClientDownload.EncryptedZipUsesAes",
                              has_aes_encrypted);
  }

  if (timeout) {
    results->analysis_result = ArchiveAnalysisResult::kTimeout;
  } else if (reader.ok()) {
    results->analysis_result = ArchiveAnalysisResult::kValid;
  } else {
    results->analysis_result = ArchiveAnalysisResult::kFailedDuringIteration;
  }

  results->success = reader.ok() && !timeout;
}

}  // namespace zip_analyzer
}  // namespace safe_browsing
