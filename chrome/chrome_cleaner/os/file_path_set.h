// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_FILE_PATH_SET_H_
#define CHROME_CHROME_CLEANER_OS_FILE_PATH_SET_H_

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/files/file_path.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"

namespace chrome_cleaner {

typedef std::unordered_set<base::FilePath, std::hash<base::FilePath>>
    UnorderedFilePathSet;

// Use this class to store sets of |FilePath|s to be searched. It takes care of
// properly comparing paths which may need to be converted to long paths.
class FilePathSet {
 public:
  FilePathSet();
  FilePathSet(const FilePathSet& file_path_set);

  // Create a file path set from a list of string paths.
  FilePathSet(std::initializer_list<const wchar_t*> path_list);

  ~FilePathSet();

  FilePathSet& operator=(const FilePathSet& file_path_set);

  // Insert a |file_path| in |file_paths_| after proper conversion.
  bool Insert(const base::FilePath& file_path);
  // Return true if properly converted |file_path| is contained in
  // |file_paths_|.
  bool Contains(const base::FilePath& file_path) const;

  // Similar to the std::set API.
  void clear() { file_paths_.clear(); }
  void erase(const base::FilePath& file_path) { file_paths_.erase(file_path); }
  bool empty() const { return file_paths_.empty(); }
  size_t size() const { return file_paths_.size(); }
  bool operator==(const FilePathSet& other) const;

  // Don't expose the insertion of iterated elements from any other type than
  // |FilePathSet| so that we don't have to ensure the paths are canonical.
  void CopyFrom(const FilePathSet& other) {
    file_paths_.insert(other.file_paths_.begin(), other.file_paths_.end());
  }

  // Removes from |file_paths_| all files that don't exist.
  void DiscardNonExistingFiles();

  // Read only access to the content.
  const UnorderedFilePathSet& file_paths() const { return file_paths_; }

  // Returns all the paths in reverse order, so that the folder contents will
  // come before the folder.
  const std::vector<base::FilePath> ReverseSorted() const;

  // Returns all paths in a vector.
  const std::vector<base::FilePath> ToVector() const;

 private:
  UnorderedFilePathSet file_paths_;
};

template <typename V>
class FilePathMap {
 public:
  typedef std::unordered_map<base::FilePath, V, std::hash<base::FilePath>>
      MapType;

  FilePathMap() = default;
  FilePathMap(const FilePathMap&) = default;
  ~FilePathMap() = default;

  bool Insert(const base::FilePath& path, const V& value) {
    return file_path_map_.insert(std::make_pair(NormalizePath(path), value))
        .second;
  }

  void CopyFrom(const FilePathMap& other) {
    return file_path_map_.insert(other.file_path_map_.begin(),
                                 other.file_path_map_.end());
  }

  V* Find(const base::FilePath& path) {
    typename MapType::iterator it = file_path_map_.find(path);
    if (it == file_path_map_.end())
      it = file_path_map_.find(NormalizePath(path));
    if (it == file_path_map_.end())
      return nullptr;
    else
      return &(it->second);
  }

  const MapType& map() const { return file_path_map_; }

 private:
  MapType file_path_map_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_FILE_PATH_SET_H_
