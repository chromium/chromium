// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_FILE_UTIL_SAFE_ARCHIVE_ANALYZER_H_
#define CHROME_SERVICES_FILE_UTIL_SAFE_ARCHIVE_ANALYZER_H_

#include <optional>

#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/services/file_util/public/mojom/safe_archive_analyzer.mojom.h"
#include "chrome/utility/safe_browsing/seven_zip_analyzer.h"
#include "chrome/utility/safe_browsing/zip_analyzer.h"
#include "mojo/public/cpp/bindings/remote.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/utility/safe_browsing/mac/dmg_analyzer.h"
#endif

#if USE_UNRAR
#include "chrome/utility/safe_browsing/rar_analyzer.h"
#endif

namespace base {
class File;
}

using AnalysisFinishedCallback = base::OnceCallback<void()>;
using GetTempFileCallback = base::OnceCallback<void(base::File)>;

class SafeArchiveAnalyzer : public chrome::mojom::SafeArchiveAnalyzer {
 public:
  SafeArchiveAnalyzer();

  SafeArchiveAnalyzer(const SafeArchiveAnalyzer&) = delete;
  SafeArchiveAnalyzer& operator=(const SafeArchiveAnalyzer&) = delete;

  ~SafeArchiveAnalyzer() override;

 private:
  // chrome::mojom::SafeArchiveAnalyzer:
  void AnalyzeZipFile(
      base::File zip_file,
      const std::optional<std::string>& password,
      mojo::PendingRemote<chrome::mojom::TemporaryFileGetter> temp_file_getter,
      AnalyzeZipFileCallback callback) override;
  void AnalyzeDmgFile(
      base::File dmg_file,
      mojo::PendingRemote<chrome::mojom::TemporaryFileGetter> temp_file_getter,
      AnalyzeDmgFileCallback callback) override;
  void AnalyzeRarFile(
      base::File rar_file,
      const std::optional<std::string>& password,
      mojo::PendingRemote<chrome::mojom::TemporaryFileGetter> temp_file_getter,
      AnalyzeRarFileCallback callback) override;
  void AnalyzeSevenZipFile(
      base::File seven_zip_file,
      mojo::PendingRemote<chrome::mojom::TemporaryFileGetter> temp_file_getter,
      AnalyzeSevenZipFileCallback callback) override;

  // Uses `temp_file_getter_` to supply a temporary file to callback.
  void RequestTemporaryFile(GetTempFileCallback callback);

  // Evokes the main callback to tell the browser process that the
  // archive is finished downloading. Takes a FilePath to match the nested
  // analyzer functions.
  void AnalysisFinished(base::FilePath path);

  // A timeout to ensure the SafeArchiveAnalyzer does not take too much time.
  void Timeout();

  safe_browsing::ZipAnalyzer zip_analyzer_;
  safe_browsing::SevenZipAnalyzer seven_zip_analyzer_;
#if BUILDFLAG(IS_MAC)
  safe_browsing::dmg::DMGAnalyzer dmg_analyzer_;
#endif
#if USE_UNRAR
  safe_browsing::RarAnalyzer rar_analyzer_;
#endif

  // A timer to ensure no archive takes too long to unpack.
  base::OneShotTimer timeout_timer_;

  base::OnceCallback<void(const safe_browsing::ArchiveAnalyzerResults&)>
      callback_;
  safe_browsing::ArchiveAnalyzerResults results_;
  mojo::Remote<chrome::mojom::TemporaryFileGetter> temp_file_getter_;
  base::WeakPtrFactory<SafeArchiveAnalyzer> weak_factory_{this};
};

#endif  // CHROME_SERVICES_FILE_UTIL_SAFE_ARCHIVE_ANALYZER_H_
