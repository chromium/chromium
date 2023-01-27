// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/seven_zip_analyzer.h"

#include "base/files/memory_mapped_file.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/lzma_sdk/google/seven_zip_reader.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
// Must be after <windows.h>
#include <winbase.h>
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include <sys/mman.h>
#endif

namespace safe_browsing::seven_zip_analyzer {

namespace {

constexpr base::TimeDelta kAnalysisTimeout = base::Seconds(10);

class SevenZipDelegate : public seven_zip::Delegate {
 public:
  SevenZipDelegate(ArchiveAnalyzerResults* results,
                   base::File temp_file,
                   base::File temp_file2)
      : results_(results),
        temp_file_(std::move(temp_file)),
        temp_file2_(std::move(temp_file2)) {
    results_->success = false;
    results_->analysis_result = ArchiveAnalysisResult::kUnknown;
    results_->file_count = 0;
    results_->directory_count = 0;
  }

  void OnOpenError(seven_zip::Result result) override { success_ = false; }

  base::File OnTempFileRequest() override { return std::move(temp_file2_); }

  bool OnEntry(const seven_zip::EntryInfo& entry,
               base::span<uint8_t>& output) override {
    if (base::TimeTicks::Now() - start_time_ > kAnalysisTimeout) {
      results_->success = false;
      results_->analysis_result = ArchiveAnalysisResult::kTimeout;
      return false;
    }

    if (entry.file_size == 0) {
      // Empty files try to initialize the memory mapping with region {0, 0},
      // which is confused with Region::kWholeFile. Since we can't truncate the
      // file within the utility process sandbox, the file still has contents
      // from a previous entry, and we end up mapping those contents. This leads
      // to CHECK failures since `output.size()` does not match
      // `entry.file_size`. Since the file is actually empty, we can skip the
      // memory mapping here to avoid this.
      output = base::span<uint8_t>();
      return true;
    }

    mapped_file_.emplace();
    bool mapped_file_ok = mapped_file_->Initialize(
        temp_file_.Duplicate(), {0, static_cast<size_t>(entry.file_size)},
        base::MemoryMappedFile::READ_WRITE_EXTEND);
    if (!mapped_file_ok) {
      results_->success = false;
      results_->analysis_result = ArchiveAnalysisResult::kUnknown;
      return false;
    }

    output = base::span<uint8_t>(mapped_file_->data(), mapped_file_->length());
    return true;
  }

  bool OnDirectory(const seven_zip::EntryInfo& entry) override {
    if (base::TimeTicks::Now() - start_time_ > kAnalysisTimeout) {
      results_->success = false;
      results_->analysis_result = ArchiveAnalysisResult::kTimeout;
      return false;
    }

    UpdateArchiveAnalyzerResultsWithFile(entry.file_path, &temp_file_,
                                         entry.file_size, entry.is_encrypted,
                                         results_);
    results_->directory_count++;
    return true;
  }

  bool EntryDone(seven_zip::Result result,
                 const seven_zip::EntryInfo& entry) override {
    base::UmaHistogramEnumeration("SBClientDownload.SevenZipEntryResult",
                                  result);

    if (base::TimeTicks::Now() - start_time_ > kAnalysisTimeout) {
      results_->success = false;
      results_->analysis_result = ArchiveAnalysisResult::kTimeout;
      return false;
    }

    results_->file_count++;

    // Since unpacking an encrypted entry is expected to fail, allow all results
    // here for encrypted entries.
    if (result == seven_zip::Result::kSuccess || entry.is_encrypted) {
      // TODO(crbug/1373509): We have the entire file in memory, so it's silly
      // to do all this work to flush it and read it back. Can we simplify this
      // process? This also reduces the risk that the file is not flushed fully.
      mapped_file_.reset();
      UpdateArchiveAnalyzerResultsWithFile(entry.file_path, &temp_file_,
                                           entry.file_size, entry.is_encrypted,
                                           results_);
    } else {
      success_ = false;
    }

    return true;
  }

  bool success() const { return success_; }

 private:
  const raw_ptr<ArchiveAnalyzerResults> results_;
  base::File temp_file_;
  base::File temp_file2_;
  const base::TimeTicks start_time_{base::TimeTicks::Now()};
  bool success_ = true;
  absl::optional<base::MemoryMappedFile> mapped_file_;
};

}  // namespace

void AnalyzeSevenZipFile(base::File seven_zip_file,
                         base::File temp_file,
                         base::File temp_file2,
                         ArchiveAnalyzerResults* results) {
  bool too_big_to_unpack =
      base::checked_cast<uint64_t>(seven_zip_file.GetLength()) >
      FileTypePolicies::GetInstance()->GetMaxFileSizeToAnalyze("7z");
  if (too_big_to_unpack) {
    results->success = false;
    results->analysis_result = ArchiveAnalysisResult::kTooLarge;
    return;
  }

  SevenZipDelegate delegate(results, std::move(temp_file),
                            std::move(temp_file2));
  seven_zip::Extract(std::move(seven_zip_file), delegate);

  if (delegate.success()) {
    results->success = true;
    results->analysis_result = ArchiveAnalysisResult::kValid;
  }
}

}  // namespace safe_browsing::seven_zip_analyzer
