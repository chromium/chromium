// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/processing/processing_utils.h"

#include "base/strings/string_util.h"

namespace segmentation_platform::processing {

// Process OS version string to return os version number as int.
int ProcessOsVersionString(std::string os_version) {
  // Special case handling for `os_version` string.
  if (os_version == "5.0.99") {
    return 6;
  }
  if (os_version == "6.0.99") {
    return 7;
  }
  if (os_version == "7.0.99") {
    return 8;
  }
  if (os_version == "8.0.99") {
    return 9;
  }
  if (os_version == "9.0.99") {
    return 10;
  }
  if (os_version == "10.0.99") {
    return 11;
  }

  // Note: we sometimes see some special cases (such as "10.0") with
  // non-trivial number of clients (e.g. 12k). But when such cases represent a
  // trivial minority (e.g. "10.0" is 0.02% of the "10" clients), we conclude
  // that the special version isn't mainstream and we prefer to leave the OS
  // version as unknown.

  // For version starting from 10 or above with dots.
  int os_version_number = 0;
  size_t found = os_version.find(".");
  if (found != std::string::npos) {
    if (base::StringToInt(os_version.substr(0, found), &os_version_number)) {
      return os_version_number;
    }
  }

  // Returns the best effort parsing the version number.
  base::StringToInt(os_version, &os_version_number);
  return os_version_number;
}

}  // namespace segmentation_platform::processing
