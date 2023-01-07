// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_MODEL_OFFLINE_PAGE_ITEM_GENERATOR_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_MODEL_OFFLINE_PAGE_ITEM_GENERATOR_H_

#include <string>

#include "base/time/time.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_archive_publisher.h"
#include "components/offline_pages/core/offline_page_item.h"

class GURL;

namespace offline_pages {

// Class for generating offline page items for testing.
class OfflinePageItemGenerator {
 public:
  OfflinePageItemGenerator();
  ~OfflinePageItemGenerator();

  OfflinePageItem CreateItem();
  // Creating item along with a file in |temp_dir_|.
  // Make sure to set |temp_dir_| before calling this method.
  OfflinePageItem CreateItemWithTempFile();

  void SetNamespace(const std::string& name_space);
  void SetId(const std::string& id);
  void SetRequestOrigin(const std::string& request_origin);
  void SetUrl(const GURL& url);
  void SetOriginalUrl(const GURL& url);
  void SetFileSize(int64_t file_size);
  void SetCreationTime(base::Time time);
  void SetLastAccessTime(base::Time time);
  void SetAccessCount(int access_count);
  void SetArchiveDirectory(const base::FilePath& archive_dir);
  void SetDigest(const std::string& digest);
  void SetFileMissingTime(base::Time file_missing_time);
  void SetUseOfflineIdAsSystemDownloadId(bool enable);
  void SetSystemDownloadId(int64_t system_download_id);

 private:
  std::string namespace_ = kDefaultNamespace;
  std::string id_;
  std::string request_origin_;
  GURL url_;
  GURL original_url_;
  int64_t file_size_ = 0;
  base::Time creation_time_;
  base::Time last_access_time_;
  int access_count_ = 0;
  base::FilePath archive_dir_;
  std::string digest_;
  base::Time file_missing_time_;
  int64_t system_download_id_ = kArchiveNotPublished;

  bool use_offline_id_as_system_download_id_ = false;
};
}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_MODEL_OFFLINE_PAGE_ITEM_GENERATOR_H_
