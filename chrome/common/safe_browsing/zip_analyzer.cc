// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/zip_analyzer.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>

#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/ranges.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/common/safe_browsing/file_type_policies.h"
#include "components/safe_browsing/proto/csd.pb.h"
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
    return;
  }

  bool too_big_to_unpack =
      base::checked_cast<uint64_t>(zip_file.GetLength()) >
      FileTypePolicies::GetInstance()->GetMaxFileSizeToAnalyze("zip");
  if (too_big_to_unpack) {
    results->success = false;
    return;
  }

  bool timeout = false;
  bool advanced = true;
  results->file_count = 0;
  results->directory_count = 0;
  for (; reader.HasMore(); advanced = reader.AdvanceToNextEntry()) {
    if (!advanced) {
      DVLOG(1) << "Could not advance to next entry, aborting zip scan.";
      return;
    }
    if (!reader.OpenCurrentEntryInZip()) {
      DVLOG(1) << "Failed to open current entry in zip file";
      continue;
    }
    if (base::Time::Now() - start_time >
        base::TimeDelta::FromMilliseconds(kZipAnalysisTimeoutMs)) {
      timeout = true;
      break;
    }

    // Clear the |temp_file| between extractions.
    temp_file.Seek(base::File::Whence::FROM_BEGIN, 0);
    temp_file.SetLength(0);
    zip::FileWriterDelegate writer(&temp_file);
    reader.ExtractCurrentEntry(&writer, std::numeric_limits<uint64_t>::max());
    UpdateArchiveAnalyzerResultsWithFile(
        reader.current_entry_info()->file_path(), &temp_file,
        writer.file_length(), reader.current_entry_info()->is_encrypted(),
        results);

    if (reader.current_entry_info()->is_directory())
      results->directory_count++;
    else
      results->file_count++;
  }

  results->success = !timeout;
}

}  // namespace zip_analyzer
}  // namespace safe_browsing
