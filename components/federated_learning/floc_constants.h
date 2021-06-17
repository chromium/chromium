// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEDERATED_LEARNING_FLOC_CONSTANTS_H_
#define COMPONENTS_FEDERATED_LEARNING_FLOC_CONSTANTS_H_

#include "base/files/file_path.h"

namespace federated_learning {

extern const uint8_t kMaxNumberOfBitsInFloc;

// ---------------------------
// For the preferences storage
// ---------------------------
extern const char kFlocIdValuePrefKey[];
extern const char kFlocIdStatusPrefKey[];
extern const char kFlocIdHistoryBeginTimePrefKey[];
extern const char kFlocIdHistoryEndTimePrefKey[];
extern const char kFlocIdFinchConfigVersionPrefKey[];
extern const char kFlocIdSortingLshVersionPrefKey[];
extern const char kFlocIdComputeTimePrefKey[];
extern const char kManifestFlocComponentFormatKey[];

// -----------------------------
// For the sorting-lsh algorithm
// -----------------------------
extern const uint8_t kSortingLshMaxBits;
extern const uint32_t kSortingLshBlockedMask;
extern const uint32_t kSortingLshSizeMask;

// ------------------------------
// For the floc component updater
// ------------------------------
extern const int kCurrentFlocComponentFormatVersion;

// The name of the top-level directory under the user data directory that
// contains all files and subdirectories related to the floc.
extern const base::FilePath::CharType kTopLevelDirectoryName[];

// The name of the file that stores the sorting-lsh clusters. The file lives
// under under |kTopLevelDirectoryName|
extern const base::FilePath::CharType kSortingLshClustersFileName[];

}  // namespace federated_learning

#endif  // COMPONENTS_FEDERATED_LEARNING_FLOC_CONSTANTS_H_