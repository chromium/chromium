// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_BROWSER_FILE_MANAGER_H_
#define COMPONENTS_PAINT_PREVIEW_BROWSER_FILE_MANAGER_H_

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace paint_preview {

// Manages paint preview files associated with a root directory (typically a
// user profile).
class FileManager {
 public:
  // Create a file manager for |root_directory|. Top level items in
  // |root_directoy| should be exclusively managed by this class. Items within
  // the subdirectories it creates can be freely modified.
  explicit FileManager(const base::FilePath& root_directory);
  ~FileManager();

  // Get statistics about the time of creation and size of artifacts.
  size_t GetSizeOfArtifactsFor(const GURL& url);
  bool GetCreatedTime(const GURL& url, base::Time* created_time);
  bool GetLastModifiedTime(const GURL& url, base::Time* last_modified_time);

  // Creates or gets a subdirectory under |root_directory|/ for |url| and
  // assigns it to |directory|. Returns true on success. If the directory was
  // compressed then it is uncompressed automatically.
  bool CreateOrGetDirectoryFor(const GURL& url, base::FilePath* directory);

  // Compresses the directory associated with |url|. Returns true on success or
  // if the directory was already compressed.
  // NOTE: an empty directory or a directory containing only empty
  // files/directories will not compress.
  bool CompressDirectoryFor(const GURL& url);

  // Deletes artifacts associated with |urls|.
  void DeleteArtifactsFor(const std::vector<GURL>& urls);

  // Deletes all stored paint previews stored in the |profile_directory_|.
  void DeleteAll();

 private:
  enum StorageType {
    kNone = 0,
    kDirectory = 1,
    kZip = 2,
  };

  StorageType GetPathForUrl(const GURL& url, base::FilePath* path);

  base::FilePath root_directory_;

  FileManager(const FileManager&) = delete;
  FileManager& operator=(const FileManager&) = delete;
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_BROWSER_FILE_MANAGER_H_
