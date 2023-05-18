// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/download_row.h"

#include "components/history/core/browser/download_constants.h"

namespace history {

DownloadRow::DownloadRow() = default;
DownloadRow::DownloadRow(const DownloadRow& other) = default;
DownloadRow::DownloadRow(DownloadRow&& other) = default;
DownloadRow::~DownloadRow() = default;

DownloadRow& DownloadRow::operator=(const DownloadRow& other) = default;

bool DownloadRow::operator==(const DownloadRow& rhs) const {
  return current_path == rhs.current_path && target_path == rhs.target_path &&
         url_chain == rhs.url_chain && referrer_url == rhs.referrer_url &&
         site_url == rhs.site_url &&
         embedder_download_data == rhs.embedder_download_data &&
         tab_url == rhs.tab_url && tab_referrer_url == rhs.tab_referrer_url &&
         http_method == rhs.http_method && mime_type == rhs.mime_type &&
         original_mime_type == rhs.original_mime_type &&
         start_time == rhs.start_time && end_time == rhs.end_time &&
         etag == rhs.etag && last_modified == rhs.last_modified &&
         received_bytes == rhs.received_bytes &&
         total_bytes == rhs.total_bytes && state == rhs.state &&
         danger_type == rhs.danger_type &&
         interrupt_reason == rhs.interrupt_reason && hash == rhs.hash &&
         id == rhs.id && guid == rhs.guid && opened == rhs.opened &&
         last_access_time == rhs.last_access_time &&
         transient == rhs.transient && by_ext_id == rhs.by_ext_id &&
         by_ext_name == rhs.by_ext_name && by_web_app_id == rhs.by_web_app_id &&
         download_slice_info == rhs.download_slice_info;
}

}  // namespace history
