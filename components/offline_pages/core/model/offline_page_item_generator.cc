// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/offline_page_item_generator.h"

#include "base/files/file_util.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "url/gurl.h"

namespace offline_pages {

OfflinePageItemGenerator::OfflinePageItemGenerator() = default;

OfflinePageItemGenerator::~OfflinePageItemGenerator() {}

OfflinePageItem OfflinePageItemGenerator::CreateItem() {
  OfflinePageItem item;
  item.offline_id = store_utils::GenerateOfflineId();
  item.client_id.name_space = namespace_;
  if (id_.empty())
    item.client_id.id = base::NumberToString(item.offline_id);
  else
    item.client_id.id = id_;
  item.request_origin = request_origin_;
  item.url = url_;
  item.original_url_if_different = original_url_;
  item.file_size = file_size_;
  item.creation_time = creation_time_;
  item.last_access_time = last_access_time_;
  item.access_count = access_count_;
  item.digest = digest_;
  item.file_missing_time = file_missing_time_;
  if (use_offline_id_as_system_download_id_)
    item.system_download_id = item.offline_id;
  else
    item.system_download_id = system_download_id_;

  return item;
}

OfflinePageItem OfflinePageItemGenerator::CreateItemWithTempFile() {
  // If hitting this DCHECK, please call SetArchiveDirectory before calling
  // this method for creating files with page.
  DCHECK(!archive_dir_.empty());
  OfflinePageItem item = CreateItem();
  base::FilePath path;
  base::CreateTemporaryFileInDir(archive_dir_, &path);
  base::FilePath mhtml_path = path.AddExtension(FILE_PATH_LITERAL("mhtml"));
  bool move_result = base::Move(path, mhtml_path);
  DCHECK(move_result);
  item.file_path = mhtml_path;
  return item;
}

void OfflinePageItemGenerator::SetNamespace(const std::string& name_space) {
  namespace_ = name_space;
}

void OfflinePageItemGenerator::SetId(const std::string& id) {
  id_ = id;
}

void OfflinePageItemGenerator::SetRequestOrigin(
    const std::string& request_origin) {
  request_origin_ = request_origin;
}

void OfflinePageItemGenerator::SetUrl(const GURL& url) {
  url_ = url;
}

void OfflinePageItemGenerator::SetOriginalUrl(const GURL& url) {
  original_url_ = url;
}

void OfflinePageItemGenerator::SetFileSize(int64_t file_size) {
  file_size_ = file_size;
}

void OfflinePageItemGenerator::SetCreationTime(base::Time creation_time) {
  creation_time_ = creation_time;
}

void OfflinePageItemGenerator::SetLastAccessTime(base::Time last_access_time) {
  last_access_time_ = last_access_time;
}

void OfflinePageItemGenerator::SetAccessCount(int access_count) {
  access_count_ = access_count;
}

void OfflinePageItemGenerator::SetArchiveDirectory(
    const base::FilePath& archive_dir) {
  archive_dir_ = archive_dir;
}

void OfflinePageItemGenerator::SetDigest(const std::string& digest) {
  digest_ = digest;
}

void OfflinePageItemGenerator::SetFileMissingTime(
    base::Time file_missing_time) {
  file_missing_time_ = file_missing_time;
}

void OfflinePageItemGenerator::SetUseOfflineIdAsSystemDownloadId(bool enable) {
  use_offline_id_as_system_download_id_ = enable;
}

void OfflinePageItemGenerator::SetSystemDownloadId(int64_t system_download_id) {
  system_download_id_ = system_download_id;
}

}  // namespace offline_pages
