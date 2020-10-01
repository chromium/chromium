// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEDERATED_LEARNING_FLOC_CONSTANTS_H_
#define COMPONENTS_FEDERATED_LEARNING_FLOC_CONSTANTS_H_

#include "base/files/file_path.h"

namespace federated_learning {

extern const uint8_t kMaxNumberOfBitsInFloc;

extern const char kManifestFlocComponentFormatKey[];

extern const int kCurrentFlocComponentFormatVersion;

// The name of the top-level directory under the user data directory that
// contains all files and subdirectories related to the floc.
extern const base::FilePath::CharType kTopLevelDirectoryName[];

// Paths under |kTopLevelDirectoryName|
// ------------------------------------

// The name of the file that stores the blocklist.
extern const base::FilePath::CharType kBlocklistFileName[];

// The name of the file that stores the sorting-lsh clusters.
extern const base::FilePath::CharType kSortingLshClustersFileName[];

}  // namespace federated_learning

#endif  // COMPONENTS_FEDERATED_LEARNING_FLOC_CONSTANTS_H_