// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_COMMON_FILES_SCAN_DATA_H_
#define COMPONENTS_ENTERPRISE_COMMON_FILES_SCAN_DATA_H_

#include <map>
#include <set>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/clipboard/file_info.h"

namespace enterprise_connectors {

// Helper class to handle files going through content analysis by expanding
// directories, aggregating verdicts, etc.
class FilesScanData final {
 public:
  // Represents an expansion of the paths in `base_paths_` where each directory
  // has been traversed to include each sub-file as a key. The map value
  // represents the index in `base_paths_` for the parent entry of that file.
  // For instance this means that for a `base_paths_` of [ "a.txt", "dir/"],
  // `ExpandedPathsIndexes` might be populated with { "a.txt": 0,
  // "dir/sub_1.txt": 1, "dir/sub_2.txt": 1 }.
  using ExpandedPathsIndexes = std::map<base::FilePath, size_t>;

  // Used internally by FilesScanData to return information about the path
  // expansion.  This structure is public because it is used by the anonymous
  // GetPathsToScan() function in the implementation file.
  struct PathsToScanResult {
    PathsToScanResult(
        std::vector<base::FilePath> base_paths,
        FilesScanData::ExpandedPathsIndexes expanded_paths_indexes,
        std::vector<base::FilePath> paths);
    PathsToScanResult(const PathsToScanResult&) = delete;
    PathsToScanResult(PathsToScanResult&&);
    PathsToScanResult& operator=(const PathsToScanResult&) = delete;
    PathsToScanResult& operator=(PathsToScanResult&&);
    ~PathsToScanResult();

    std::vector<base::FilePath> base_paths;
    FilesScanData::ExpandedPathsIndexes expanded_paths_indexes;
    std::vector<base::FilePath> paths;
  };

  FilesScanData();
  explicit FilesScanData(std::vector<ui::FileInfo> paths);
  explicit FilesScanData(std::vector<base::FilePath> paths);
  FilesScanData(const FilesScanData&) = delete;
  FilesScanData& operator=(const FilesScanData&) = delete;
  ~FilesScanData();

  // Starts a task on a background thread to traverse `base_paths_` directories
  // and build of map of all sub-files. The result is stored into
  // `expanded_paths_indexes_` and `expanded_paths_`.
  void ExpandPaths(base::OnceClosure done_closure);

  // Returns a set indicating which paths in `base_paths_` should be blocked
  // due to content analysis violations based on `expanded_paths_` verdicts. The
  // size of `allowed_paths` and its indexes are expected to match
  // `expanded_paths_`.
  std::set<size_t> IndexesToBlock(const std::vector<bool>& allowed_paths);

  // Once ExpandPaths() is called, accessing base paths is not allowed until
  // the done closure is called.  After take_base_paths() is called, further
  // calls to get the base paths will return empty vectors.
  const std::vector<base::FilePath>& base_paths() const;
  std::vector<base::FilePath> take_base_paths();

  const ExpandedPathsIndexes& expanded_paths_indexes() const;
  const std::vector<base::FilePath>& expanded_paths() const;

 private:
  void OnExpandPathsDone(PathsToScanResult result);

  // The file paths given as input for a scan. This does not include any
  // expansion of directories.
  std::vector<base::FilePath> base_paths_;

  // The following members contain the result of an `ExpandPaths()` call.
  ExpandedPathsIndexes expanded_paths_indexes_;
  std::vector<base::FilePath> expanded_paths_;

  // Called after the `ExpandPaths()` operation is done to tell callers its
  // results can be used.
  base::OnceClosure expand_paths_done_closure_;

  base::WeakPtrFactory<FilesScanData> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_COMMON_FILES_SCAN_DATA_H_
