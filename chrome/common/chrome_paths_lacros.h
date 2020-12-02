// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_PATHS_LACROS_H_
#define CHROME_COMMON_CHROME_PATHS_LACROS_H_

namespace base {
class FilePath;
}  // namespace base

namespace chrome {

// Sets the default paths for user documents and downloads. The paths are sent
// by ash-chrome and are set early in lacros-chrome startup.
void SetLacrosDefaultPaths(const base::FilePath& documents_dir,
                           const base::FilePath& downloads_dir);

}  // namespace chrome

#endif  // CHROME_COMMON_CHROME_PATHS_LACROS_H_
