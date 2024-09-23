// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/common/files_scan_data.h"

#include <utility>

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/task/thread_pool.h"

namespace enterprise_connectors {

namespace {

FilesScanData::PathsToScanResult GetPathsToScan(
    std::vector<base::FilePath> base_paths) {
  FilesScanData::ExpandedPathsIndexes expanded_paths_indexes;
  std::vector<base::FilePath> paths;
  for (size_t i = 0; i < base_paths.size(); ++i) {
    const base::FilePath& file = base_paths.at(i);
    base::File::Info info;

    // Ignore the path if it's a symbolic link.
    if (!base::GetFileInfo(file, &info) || info.is_symbolic_link)
      continue;

    // If the file is a directory, recursively add the files it holds to `data`.
    if (info.is_directory) {
      base::FileEnumerator file_enumerator(file, /*recursive=*/true,
                                           base::FileEnumerator::FILES);
      for (base::FilePath sub_path = file_enumerator.Next(); !sub_path.empty();
           sub_path = file_enumerator.Next()) {
        paths.push_back(sub_path);
        expanded_paths_indexes.insert({sub_path, i});
      }
    } else {
      paths.push_back(file);
      expanded_paths_indexes.insert({file, i});
    }
  }

  return {std::move(base_paths), std::move(expanded_paths_indexes),
          std::move(paths)};
}

}  // namespace

FilesScanData::PathsToScanResult::PathsToScanResult(
    std::vector<base::FilePath> base_paths,
    FilesScanData::ExpandedPathsIndexes expanded_paths_indexes,
    std::vector<base::FilePath> paths)
    : base_paths(std::move(base_paths)),
      expanded_paths_indexes(std::move(expanded_paths_indexes)),
      paths(std::move(paths)) {}

FilesScanData::PathsToScanResult::PathsToScanResult(PathsToScanResult&&) =
    default;

FilesScanData::PathsToScanResult& FilesScanData::PathsToScanResult::operator=(
    PathsToScanResult&&) = default;

FilesScanData::PathsToScanResult::~PathsToScanResult() = default;

FilesScanData::FilesScanData() = default;

FilesScanData::FilesScanData(std::vector<ui::FileInfo> paths) {
  base_paths_.reserve(paths.size());
  for (const ui::FileInfo& file_info : paths) {
    base_paths_.push_back(file_info.path);
  }
}

FilesScanData::FilesScanData(std::vector<base::FilePath> paths)
    : base_paths_(std::move(paths)) {}

FilesScanData::~FilesScanData() = default;

void FilesScanData::ExpandPaths(base::OnceClosure done_closure) {
  expand_paths_done_closure_ = std::move(done_closure);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&GetPathsToScan, std::move(base_paths_)),
      base::BindOnce(&FilesScanData::OnExpandPathsDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

std::set<size_t> FilesScanData::IndexesToBlock(
    const std::vector<bool>& allowed_paths) {
  if (allowed_paths.size() != expanded_paths_indexes_.size() ||
      expanded_paths_.size() != allowed_paths.size()) {
    return {};
  }

  std::set<size_t> indexes_to_block;
  for (size_t i = 0; i < allowed_paths.size(); ++i) {
    if (allowed_paths[i])
      continue;
    indexes_to_block.insert(expanded_paths_indexes_.at(expanded_paths_[i]));
  }
  return indexes_to_block;
}

const std::vector<base::FilePath>& FilesScanData::base_paths() const {
  return base_paths_;
}

std::vector<base::FilePath> FilesScanData::take_base_paths() {
  return std::move(base_paths_);
}

const FilesScanData::ExpandedPathsIndexes&
FilesScanData::expanded_paths_indexes() const {
  return expanded_paths_indexes_;
}

const std::vector<base::FilePath>& FilesScanData::expanded_paths() const {
  return expanded_paths_;
}

void FilesScanData::OnExpandPathsDone(PathsToScanResult result) {
  base_paths_ = std::move(result.base_paths);
  expanded_paths_indexes_ = std::move(result.expanded_paths_indexes);
  expanded_paths_ = std::move(result.paths);
  std::move(expand_paths_done_closure_).Run();
}

}  // namespace enterprise_connectors
