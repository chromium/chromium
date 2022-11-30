// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_page_item.h"

namespace offline_pages {

OfflinePageItem::OfflinePageItem() = default;

OfflinePageItem::OfflinePageItem(const GURL& url,
                                 int64_t offline_id,
                                 const ClientId& client_id,
                                 const base::FilePath& file_path,
                                 int64_t file_size)
    : url(url),
      offline_id(offline_id),
      client_id(client_id),
      file_path(file_path),
      file_size(file_size) {}

OfflinePageItem::OfflinePageItem(const GURL& url,
                                 int64_t offline_id,
                                 const ClientId& client_id,
                                 const base::FilePath& file_path,
                                 int64_t file_size,
                                 const base::Time& creation_time)
    : url(url),
      offline_id(offline_id),
      client_id(client_id),
      file_path(file_path),
      file_size(file_size),
      creation_time(creation_time),
      last_access_time(creation_time) {}

OfflinePageItem::OfflinePageItem(const OfflinePageItem& other) = default;
OfflinePageItem::OfflinePageItem(OfflinePageItem&& other) = default;
OfflinePageItem::~OfflinePageItem() = default;
OfflinePageItem& OfflinePageItem::operator=(const OfflinePageItem&) = default;
OfflinePageItem& OfflinePageItem::operator=(OfflinePageItem&&) = default;

bool OfflinePageItem::operator==(const OfflinePageItem& other) const {
  return url == other.url && offline_id == other.offline_id &&
         client_id == other.client_id && file_path == other.file_path &&
         file_size == other.file_size && creation_time == other.creation_time &&
         last_access_time == other.last_access_time &&
         access_count == other.access_count && title == other.title &&
         flags == other.flags &&
         original_url_if_different == other.original_url_if_different &&
         request_origin == other.request_origin &&
         system_download_id == other.system_download_id &&
         file_missing_time == other.file_missing_time && digest == other.digest;
}

bool OfflinePageItem::operator<(const OfflinePageItem& other) const {
  return offline_id < other.offline_id;
}

}  // namespace offline_pages
