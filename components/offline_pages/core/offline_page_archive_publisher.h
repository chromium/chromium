// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_ARCHIVE_PUBLISHER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_ARCHIVE_PUBLISHER_H_

#include <cstdint>

#include "base/files/file_path.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_types.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace offline_pages {

// These constants are used to set offline_page_item.download_id when no
// download ID is available.
const int64_t kArchiveNotPublished = 0LL;
const int64_t kArchivePublishedWithoutDownloadId = -1LL;

// Identifies one published archive. Before Android Q, a published archive is
// assigned a download ID; on Q and later, a published archive is assigned a
// content URI.
struct PublishedArchiveId {
  PublishedArchiveId() = default;
  PublishedArchiveId(int64_t download_id, const base::FilePath& new_file_path)
      : download_id(download_id), new_file_path(new_file_path) {}
  bool operator==(const PublishedArchiveId& other) const {
    return download_id == other.download_id &&
           new_file_path == other.new_file_path;
  }

  // Identifier returned by Android DownloadManager when present, or
  // kArchivePublishedWithoutDownloadManager otherwise. Set to
  // kArchiveNotPublished if publishing failed.
  int64_t download_id = kArchiveNotPublished;

  // The published archive's path or content URI; empty if publishing failed.
  base::FilePath new_file_path;
};

// The result of publishing an offline page to Downloads.
struct PublishArchiveResult {
  SavePageResult move_result;
  PublishedArchiveId id;

  static PublishArchiveResult Failure(SavePageResult save_page_result);
};

// Interface of a class responsible for publishing offline page archives to
// downloads.
class OfflinePageArchivePublisher {
 public:
  using PublishArchiveDoneCallback =
      base::OnceCallback<void(const OfflinePageItem& /* offline_page */,
                              PublishArchiveResult /* archive_result */)>;

  virtual ~OfflinePageArchivePublisher() = default;

  // Publishes the page on a background thread, then returns to the
  // OfflinePageModelTaskified's done callback.
  virtual void PublishArchive(
      const OfflinePageItem& offline_page,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
      PublishArchiveDoneCallback publish_done_callback) const = 0;

  // Removes  archives from downloads.
  virtual void UnpublishArchives(
      const std::vector<PublishedArchiveId>& archive_ids) const = 0;

  virtual base::WeakPtr<OfflinePageArchivePublisher> GetWeakPtr() = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_ARCHIVE_PUBLISHER_H_
