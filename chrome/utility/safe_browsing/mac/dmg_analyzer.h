// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_SAFE_BROWSING_MAC_DMG_ANALYZER_H_
#define CHROME_UTILITY_SAFE_BROWSING_MAC_DMG_ANALYZER_H_

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/utility/safe_browsing/archive_analyzer.h"
#include "chrome/utility/safe_browsing/mac/dmg_iterator.h"
#include "chrome/utility/safe_browsing/mac/read_stream.h"
#include "components/safe_browsing/content/common/file_type_policies.h"

namespace safe_browsing {

namespace dmg {

class DMGAnalyzer : public ArchiveAnalyzer {
 public:
  DMGAnalyzer();

  ~DMGAnalyzer() override;

  DMGAnalyzer(const DMGAnalyzer&) = delete;
  DMGAnalyzer& operator=(const DMGAnalyzer&) = delete;

  // Helper function exposed for testing.
  void AnalyzeDMGFileForTesting(std::unique_ptr<DMGIterator> iterator,
                                ArchiveAnalyzerResults* results,
                                base::File temp_file,
                                FinishedAnalysisCallback callback);

 private:
  void Init() override;
  bool ResumeExtraction() override;
  base::WeakPtr<ArchiveAnalyzer> GetWeakPtr() override;

  void OnGetTempFile(base::File temp_file);

  base::File temp_file_;
  std::unique_ptr<FileReadStream> read_stream_;
  std::unique_ptr<DMGIterator> iterator_;

  base::WeakPtrFactory<DMGAnalyzer> weak_factory_{this};
};

}  // namespace dmg
}  // namespace safe_browsing

#endif  // CHROME_UTILITY_SAFE_BROWSING_MAC_DMG_ANALYZER_H_
