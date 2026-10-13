// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_module/child_module_helper.h"

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "chrome/common/chrome_constants.h"

namespace child_module {

base::FilePath GetManifestPath(const base::FilePath& version_dir) {
  if (version_dir.empty()) {
    return base::FilePath();
  }
  return version_dir.Append(kManifestFilename);
}

base::FilePath GetRendererBinaryPath(const base::FilePath& modules_dir,
                                     const base::Version& version) {
  if (modules_dir.empty() || !version.IsValid()) {
    return base::FilePath();
  }
  return modules_dir
      .AppendASCII(version.GetString())
#if BUILDFLAG(IS_WIN)
      .Append(chrome::kRendererDll);
#else
      .Append(chrome::kRendererProcessExecutableName);
#endif
}

}  // namespace child_module
