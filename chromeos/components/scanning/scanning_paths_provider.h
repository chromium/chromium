// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SCANNING_SCANNING_PATHS_PROVIDER_H_
#define CHROMEOS_COMPONENTS_SCANNING_SCANNING_PATHS_PROVIDER_H_

#include <string>

#include "base/files/file_path.h"

namespace content {
class WebUI;
}  // namespace content

namespace chromeos {

// An interface that provides the Scanning app with FilePath utils.
class ScanningPathsProvider {
 public:
  virtual ~ScanningPathsProvider() = default;

  // Gets the display name to use in the Scan To dropdown. Checks if |path| is
  // either the Google Drive root or MyFiles directory and converts it to the
  // desired display name.
  virtual std::string GetBaseNameFromPath(content::WebUI* web_ui,
                                          const base::FilePath& path) = 0;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SCANNING_SCANNING_PATHS_PROVIDER_H_
