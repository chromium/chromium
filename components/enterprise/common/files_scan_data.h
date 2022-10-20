// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_COMMON_FILES_SCAN_DATA_H_
#define COMPONENTS_ENTERPRISE_COMMON_FILES_SCAN_DATA_H_

#include <map>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/clipboard/file_info.h"

namespace enterprise_connectors {

// Helper class to handle files going through content analysis by expanding
// directories, aggregating verdicts, etc.
class FilesScanData : public base::SupportsWeakPtr<FilesScanData> {
 public:
  FilesScanData();
  explicit FilesScanData(std::vector<ui::FileInfo> paths);
  explicit FilesScanData(std::vector<base::FilePath> paths);
  FilesScanData(const FilesScanData&) = delete;
  FilesScanData& operator=(const FilesScanData&) = delete;
  ~FilesScanData();

  // Represents an expansion of the paths in `base_paths_` where each directory
  // has been traversed to include each sub-file as a key. The map value
  // represents the index in `base_paths_` for the parent entry of that file.
  // For instance this means that for a `base_paths_` of [ "a.txt", "dir/"],
  // `ExpandedPathsIndexes` might be populated with { "a.txt": 0,
  // "dir/sub_1.txt": 1, "dir/sub_2.txt": 1 }.
  using ExpandedPathsIndexes = std::map<base::FilePath, size_t>;

  // Starts a task on a background thread to traverse `base_paths_` directories
  // and build of map of all sub-files. The result is stored into
  // `expanded_paths_indexes_` and `expanded_paths_`.
  void ExpandPaths(base::OnceClosure done_closure);

  // Returns a set indicating which paths in `base_paths_` should be blocked
  // due to content analysis violations based on `expanded_paths_` verdicts. The
  // size of `allowed_paths` and its indexes are expected to match
  // `expanded_paths_`.
  std::set<size_t> IndexesToBlock(const std::vector<bool>& allowed_paths);

  const ExpandedPathsIndexes& expanded_paths_indexes();
  const std::vector<base::FilePath>& expanded_paths();

 private:
  void OnExpandPathsDone(
      std::pair<ExpandedPathsIndexes, std::vector<base::FilePath>>
          indexes_and_paths);

  // The file paths given as input for a scan. This does not include any
  // expansion of directories.
  std::vector<base::FilePath> base_paths_;

  // The following members contain the result of an `ExpandPaths()` call.
  ExpandedPathsIndexes expanded_paths_indexes_;
  std::vector<base::FilePath> expanded_paths_;

  // Called after the `ExpandPaths()` operation is done to tell callers its
  // results can be used.
  base::OnceClosure expand_paths_done_closure_;
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_COMMON_FILES_SCAN_DATA_H_
