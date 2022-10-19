// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/rar_analyzer.h"

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/i18n/streaming_utf8_validator.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/checked_math.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/common/safe_browsing/download_type_util.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "components/safe_browsing/core/common/features.h"
#include "third_party/unrar/google/unrar_wrapper.h"

namespace safe_browsing {
namespace rar_analyzer {

namespace {

// The maximum duration of RAR analysis, in milliseconds.
const int kRarAnalysisTimeoutMs = 10000;

}  // namespace

void AnalyzeRarFile(base::File rar_file,
                    base::File temp_file,
                    ArchiveAnalyzerResults* results) {
  base::Time start_time = base::Time::Now();
  results->success = false;
  results->file_count = 0;
  results->directory_count = 0;

  // If the file is too big to unpack, return failure. This will still send a
  // ping as an "invalid" RAR.
  bool too_big_to_unpack =
      base::checked_cast<uint64_t>(rar_file.GetLength()) >
      FileTypePolicies::GetInstance()->GetMaxFileSizeToAnalyze("rar");
  if (too_big_to_unpack) {
    results->analysis_result = ArchiveAnalysisResult::kTooLarge;
    return;
  }

  third_party_unrar::RarReader reader;
  if (!reader.Open(std::move(rar_file), temp_file.Duplicate())) {
    results->analysis_result = ArchiveAnalysisResult::kUnknown;
    return;
  }

  bool timeout = false;
  while (reader.ExtractNextEntry()) {
    if (base::Time::Now() - start_time >
        base::Milliseconds(kRarAnalysisTimeoutMs)) {
      timeout = true;
      break;
    }
    const third_party_unrar::RarReader::EntryInfo& entry =
        reader.current_entry();
    UpdateArchiveAnalyzerResultsWithFile(entry.file_path, &temp_file,
                                         entry.file_size, entry.is_encrypted,
                                         results);
    if (entry.is_directory)
      results->directory_count++;
    else
      results->file_count++;
  }

  results->analysis_result =
      timeout ? ArchiveAnalysisResult::kTimeout : ArchiveAnalysisResult::kValid;
  results->success = !timeout;
}

}  // namespace rar_analyzer
}  // namespace safe_browsing
