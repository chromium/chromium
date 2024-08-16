// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/safe_browsing/rar_analyzer.h"

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

RarAnalyzer::RarAnalyzer() = default;
RarAnalyzer::~RarAnalyzer() = default;

void RarAnalyzer::Init() {
  GetTempFile(
      base::BindOnce(&RarAnalyzer::OnGetTempFile, weak_factory_.GetWeakPtr()));
}

bool RarAnalyzer::ResumeExtraction() {
  while (reader_.ExtractNextEntry()) {
    const third_party_unrar::RarReader::EntryInfo& entry =
        reader_.current_entry();
    if (entry.is_encrypted && !entry.contents_valid) {
      results()->encryption_info.password_status =
          EncryptionInfo::kKnownIncorrect;
    }
    if (!UpdateResultsForEntry(temp_file_.Duplicate(),
                               GetRootPath().Append(entry.file_path),
                               entry.file_size, entry.is_encrypted,
                               entry.is_directory, entry.contents_valid)) {
      return false;
    }
  }

  if (results()->encryption_info.password_status !=
          EncryptionInfo::kKnownIncorrect &&
      results()->encryption_info.is_encrypted) {
    results()->encryption_info.password_status = EncryptionInfo::kKnownCorrect;
  }

  results()->success = true;
  results()->analysis_result = ArchiveAnalysisResult::kValid;
  return true;
}

void RarAnalyzer::OnGetTempFile(base::File temp_file) {
  if (!temp_file.IsValid()) {
    InitComplete(ArchiveAnalysisResult::kFailedToOpenTempFile);
    return;
  }

  temp_file_ = std::move(temp_file);
  // If the file is too big to unpack, return failure. This will still send a
  // ping as an "invalid" RAR.
  bool too_big_to_unpack =
      base::checked_cast<uint64_t>(GetArchiveFile().GetLength()) >
      FileTypePolicies::GetInstance()->GetMaxFileSizeToAnalyze("rar");
  if (too_big_to_unpack) {
    InitComplete(ArchiveAnalysisResult::kTooLarge);
    return;
  }

  if (password()) {
    reader_.SetPassword(*password());
  }

  // `rar_file_` is consumed by the reader and cannot be used after
  // this point.
  if (!reader_.Open(std::move(GetArchiveFile()), temp_file_.Duplicate())) {
    InitComplete(ArchiveAnalysisResult::kUnknown);
    return;
  }

  results()->encryption_info.is_encrypted |= reader_.HeadersEncrypted();
  if (IsTopLevelArchive()) {
    results()->encryption_info.is_top_level_encrypted |=
        reader_.HeadersEncrypted();
  }
  if (reader_.HeaderDecryptionFailed()) {
    results()->encryption_info.password_status =
        EncryptionInfo::kKnownIncorrect;
    InitComplete(ArchiveAnalysisResult::kUnknown);
    return;
  }

  InitComplete(ArchiveAnalysisResult::kValid);
}

base::WeakPtr<ArchiveAnalyzer> RarAnalyzer::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace safe_browsing
