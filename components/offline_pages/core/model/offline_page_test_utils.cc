// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/offline_page_test_utils.h"

#include "base/base64.h"
#include "base/files/file_enumerator.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/offline_pages/core/offline_page_archive_publisher.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_visuals.h"
#include "components/offline_pages/core/offline_store_utils.h"

namespace offline_pages {

namespace test_utils {

size_t GetFileCountInDirectory(const base::FilePath& directory) {
  base::FileEnumerator file_enumerator(directory, false,
                                       base::FileEnumerator::FILES);
  size_t count = 0;
  for (base::FilePath path = file_enumerator.Next(); !path.empty();
       path = file_enumerator.Next()) {
    count++;
  }
  return count;
}

}  // namespace test_utils

std::ostream& operator<<(std::ostream& out, const OfflinePageItem& item) {
  using base::Value;
  base::Value::Dict value;
  value.Set("url", item.url.spec());
  value.Set("offline_id", base::NumberToString(item.offline_id));
  value.Set("client_id", item.client_id.ToString());
  if (!item.file_path.empty()) {
    value.Set("file_path", item.file_path.AsUTF8Unsafe());
  }
  if (item.file_size != 0) {
    value.Set("file_size", base::NumberToString(item.file_size));
  }
  if (!item.creation_time.is_null()) {
    value.Set(
        "creation_time",
        base::NumberToString(store_utils::ToDatabaseTime(item.creation_time)));
  }
  if (!item.last_access_time.is_null()) {
    value.Set("creation_time", base::NumberToString(store_utils::ToDatabaseTime(
                                   item.last_access_time)));
  }
  if (item.access_count != 0) {
    value.Set("access_count", item.access_count);
  }
  if (!item.title.empty()) {
    value.Set("title", base::UTF16ToUTF8(item.title));
  }
  if (item.flags & OfflinePageItem::MARKED_FOR_DELETION) {
    value.Set("marked_for_deletion", true);
  }
  if (!item.original_url_if_different.is_empty()) {
    value.Set("original_url_if_different",
              item.original_url_if_different.spec());
  }
  if (!item.request_origin.empty()) {
    value.Set("request_origin", item.request_origin);
  }
  if (item.system_download_id != kArchiveNotPublished) {
    value.Set("system_download_id",
              base::NumberToString(item.system_download_id));
  }
  if (!item.file_missing_time.is_null()) {
    value.Set("file_missing_time",
              base::NumberToString(
                  store_utils::ToDatabaseTime(item.file_missing_time)));
  }
  if (!item.digest.empty()) {
    value.Set("digest", item.digest);
  }
  if (!item.snippet.empty()) {
    value.Set("snippet", item.snippet);
  }
  if (!item.attribution.empty()) {
    value.Set("attribution", item.attribution);
  }

  std::string value_string;
  base::JSONWriter::Write(value, &value_string);
  return out << value_string;
}

std::string OfflinePageVisuals::ToString() const {
  std::string thumb_data_base64 = base::Base64Encode(thumbnail);
  std::string favicon_data_base64 = base::Base64Encode(favicon);

  std::string s("OfflinePageVisuals(id=");
  s.append(base::NumberToString(offline_id)).append(", expiration=");
  s.append(base::NumberToString(store_utils::ToDatabaseTime(expiration)))
      .append(", thumbnail=");
  s.append(thumb_data_base64).append(", favicon=");
  s.append(favicon_data_base64).append(")");
  return s;
}

std::ostream& operator<<(std::ostream& out, const OfflinePageVisuals& visuals) {
  return out << visuals.ToString();
}

}  // namespace offline_pages
