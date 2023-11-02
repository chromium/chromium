// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINTS_COMPONENT_INFO_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINTS_COMPONENT_INFO_H_

#include "base/files/file_path.h"
#include "base/version.h"

namespace optimization_guide {

// Information about a version of optimization hints data received from the
// components server.
struct HintsComponentInfo {
  HintsComponentInfo(const base::Version& version, const base::FilePath& path)
      : version(version), path(path) {}

  // The version of the hints content.
  const base::Version version;

  // The path to the file containing the hints protobuf file.
  const base::FilePath path;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINTS_COMPONENT_INFO_H_
