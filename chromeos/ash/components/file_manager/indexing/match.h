// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_MATCH_H_
#define CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_MATCH_H_

#include "chromeos/ash/components/file_manager/indexing/file_info.h"

#include "base/component_export.h"

namespace ash::file_manager {

// Represents a single search match. The match consists of the file info, which
// identified the matched file and match score. The match score is a value
// between 0 and 1, indicating how good the match is. The score of 1 means a
// perfect match. Score 0 indicates a non-match.
struct COMPONENT_EXPORT(FILE_MANAGER) Match {
  Match(float score, const FileInfo& file_info);
  ~Match();

  // Returns whether this Match is equal to the `other` Match.
  bool operator==(const Match& other) const;

  // The score for this match.
  float score;

  // FileInfo of the matched file.
  FileInfo file_info;
};

}  // namespace ash::file_manager

#endif  // CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_MATCH_H_
