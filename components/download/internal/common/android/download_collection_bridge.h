// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_COMMON_ANDROID_DOWNLOAD_COLLECTION_BRIDGE_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_COMMON_ANDROID_DOWNLOAD_COLLECTION_BRIDGE_H_

#include <map>
#include <memory>
#include <string>

#include "base/files/file.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_file.h"
#include "components/download/public/common/in_progress_download_manager.h"

namespace download {

// Native class talking to the java side to publish a download to public
// download collection.
class COMPONENTS_DOWNLOAD_EXPORT DownloadCollectionBridge {
 public:
  DownloadCollectionBridge(const DownloadCollectionBridge&) = delete;
  DownloadCollectionBridge& operator=(const DownloadCollectionBridge&) = delete;

  // Creates the intermediate URI for download to write to.
  // Called on non UI thread.
  static base::FilePath CreateIntermediateUriForPublish(
      const GURL& original_url,
      const GURL& referrer_url,
      const base::FilePath& file_name,
      const std::string& mime_type);
  // Returns whether a download needs to be published.
  // Can be called on any thread.
  static bool ShouldPublishDownload(const base::FilePath& file_path);

  // Moves existing file content to the intermediate Uri, and remove
  // |source_path|.
  // Called on non UI thread.
  static DownloadInterruptReason MoveFileToIntermediateUri(
      const base::FilePath& source_path,
      const base::FilePath& destination_uri);

  // Deletes the intermediate Uri that is being written to.
  // Called on non UI thread.
  static void DeleteIntermediateUri(const base::FilePath& intermediate_uri);

  // Publishes the intermediate Uri to public download collection, and returns
  // the final Uri.
  // Called on non UI thread.
  static base::FilePath PublishDownload(const base::FilePath& intermediate_uri);

  // Opens the intermediate Uri for writing.
  // Called on non UI thread.
  static base::File OpenIntermediateUri(const base::FilePath& intermediate_uri);

  // Checks whether a file name exists.
  // Called on non UI thread.
  static bool FileNameExists(const base::FilePath& file_name);

  // Renames a content URI download to |new_display_name|. Returns true on
  // success, and false otherwise.
  // Called on non UI thread.
  static bool RenameDownloadUri(const base::FilePath& download_uri,
                                const base::FilePath& new_display_name);

  using GetDisplayNamesCallback =
      base::OnceCallback<void(InProgressDownloadManager::DisplayNames)>;
  // Gets the display name for all downloads.
  // Called on non UI thread.
  static void GetDisplayNamesForDownloads(GetDisplayNamesCallback cb);

  // Gets the display name for a download.
  static base::FilePath GetDisplayName(const base::FilePath& download_uri);

  // Add a file name for testing so that FileNameExists() will return true
  // later.
  static void AddExistingFileNameForTesting(const base::FilePath& file_name);

  // Clear all file names in the global file name set. If the set is not
  // created, create it.
  static void ResetExistingFileNamesForTesting();
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_COMMON_ANDROID_DOWNLOAD_COLLECTION_BRIDGE_H_
