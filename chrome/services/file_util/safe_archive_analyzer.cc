// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/safe_archive_analyzer.h"

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/common/safe_browsing/rar_analyzer.h"
#include "chrome/common/safe_browsing/zip_analyzer.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

#if defined(OS_MACOSX)
#include "chrome/utility/safe_browsing/mac/dmg_analyzer.h"
#endif

SafeArchiveAnalyzer::SafeArchiveAnalyzer() = default;

SafeArchiveAnalyzer::~SafeArchiveAnalyzer() = default;

void SafeArchiveAnalyzer::AnalyzeZipFile(base::File zip_file,
                                         base::File temporary_file,
                                         AnalyzeZipFileCallback callback) {
  DCHECK(temporary_file.IsValid());
  DCHECK(zip_file.IsValid());

  safe_browsing::ArchiveAnalyzerResults results;
  safe_browsing::zip_analyzer::AnalyzeZipFile(
      std::move(zip_file), std::move(temporary_file), &results);
  std::move(callback).Run(results);
}

void SafeArchiveAnalyzer::AnalyzeDmgFile(base::File dmg_file,
                                         AnalyzeDmgFileCallback callback) {
#if defined(OS_MACOSX)
  DCHECK(dmg_file.IsValid());
  safe_browsing::ArchiveAnalyzerResults results;
  safe_browsing::dmg::AnalyzeDMGFile(std::move(dmg_file), &results);
  std::move(callback).Run(results);
#else
  NOTREACHED();
#endif
}

void SafeArchiveAnalyzer::AnalyzeRarFile(base::File rar_file,
                                         base::File temporary_file,
                                         AnalyzeRarFileCallback callback) {
  DCHECK(rar_file.IsValid());

  safe_browsing::ArchiveAnalyzerResults results;
  safe_browsing::rar_analyzer::AnalyzeRarFile(
      std::move(rar_file), std::move(temporary_file), &results);
  std::move(callback).Run(results);
}
