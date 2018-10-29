// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "components/offline_pages/core/prefetch/prefetch_item.h"

namespace offline_pages {

std::string PrefetchItem::ToString() const {
  std::stringstream s;
  s << "PrefetchItem(id=" << offline_id << ", guid=" << guid << ", "
    << client_id << ", state=" << state
    << ", url=" << url.possibly_invalid_spec()
    << ", final_url=" << final_archived_url.possibly_invalid_spec()
    << ", thumbnail_url=" << thumbnail_url.possibly_invalid_spec()
    << ", favicon_url=" << favicon_url.possibly_invalid_spec()
    << ", snippet=" << snippet << ", attribution=" << attribution
    << ", gb_attempts=" << generate_bundle_attempts
    << ", get_attempts=" << get_operation_attempts
    << ", dl_attempts=" << download_initiation_attempts
    << ", operation=" << operation_name << ", body_name=" << archive_body_name
    << ", body_len=" << archive_body_length
    << ", creation_time=" << store_utils::ToDatabaseTime(creation_time)
    << ", freshness_time=" << store_utils::ToDatabaseTime(freshness_time)
    << ", error=" << error_code << ", title=" << base::UTF16ToUTF8(title)
    << ", file_path=" << file_path.AsUTF8Unsafe() << ", file_size=" << file_size
    << ")";
  return s.str();
}

std::ostream& operator<<(std::ostream& out, const PrefetchItem& pi) {
  return out << pi.ToString();
}

PrefetchItem::PrefetchItem(const PrefetchItem& other) = default;

PrefetchItem& PrefetchItem::operator=(const PrefetchItem& other) = default;

PrefetchItem& PrefetchItem::operator=(PrefetchItem&& other) = default;

bool PrefetchItem::operator==(const PrefetchItem& other) const {
  return offline_id == other.offline_id && guid == other.guid &&
         client_id == other.client_id && state == other.state &&
         url == other.url && final_archived_url == other.final_archived_url &&
         thumbnail_url == other.thumbnail_url &&
         favicon_url == other.favicon_url && snippet == other.snippet &&
         attribution == other.attribution &&
         generate_bundle_attempts == other.generate_bundle_attempts &&
         get_operation_attempts == other.get_operation_attempts &&
         download_initiation_attempts == other.download_initiation_attempts &&
         operation_name == other.operation_name &&
         archive_body_name == other.archive_body_name &&
         archive_body_length == other.archive_body_length &&
         creation_time == other.creation_time &&
         freshness_time == other.freshness_time &&
         error_code == other.error_code && title == other.title &&
         file_path == other.file_path && file_size == other.file_size;
}

bool PrefetchItem::operator!=(const PrefetchItem& other) const {
  return !(*this == other);
}

bool PrefetchItem::operator<(const PrefetchItem& other) const {
  return offline_id < other.offline_id;
}

}  // namespace offline_pages
