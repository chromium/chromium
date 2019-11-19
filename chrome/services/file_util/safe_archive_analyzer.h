// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_FILE_UTIL_SAFE_ARCHIVE_ANALYZER_H_
#define CHROME_SERVICES_FILE_UTIL_SAFE_ARCHIVE_ANALYZER_H_

#include "chrome/services/file_util/public/mojom/safe_archive_analyzer.mojom.h"

namespace base {
class File;
}

class SafeArchiveAnalyzer : public chrome::mojom::SafeArchiveAnalyzer {
 public:
  SafeArchiveAnalyzer();
  ~SafeArchiveAnalyzer() override;

 private:
  // chrome::mojom::SafeArchiveAnalyzer:
  void AnalyzeZipFile(base::File zip_file,
                      base::File temporary_file,
                      AnalyzeZipFileCallback callback) override;
  void AnalyzeDmgFile(base::File dmg_file,
                      AnalyzeDmgFileCallback callback) override;
  void AnalyzeRarFile(base::File rar_file,
                      base::File temporary_file,
                      AnalyzeRarFileCallback callback) override;

  DISALLOW_COPY_AND_ASSIGN(SafeArchiveAnalyzer);
};

#endif  // CHROME_SERVICES_FILE_UTIL_SAFE_ARCHIVE_ANALYZER_H_
