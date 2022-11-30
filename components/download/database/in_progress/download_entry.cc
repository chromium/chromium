// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/database/in_progress/download_entry.h"

namespace download {

DownloadEntry::DownloadEntry() = default;

DownloadEntry::DownloadEntry(const DownloadEntry& other) = default;

DownloadEntry::DownloadEntry(
    const std::string& guid,
    const std::string& request_origin,
    DownloadSource download_source,
    bool fetch_error_body,
    const DownloadUrlParameters::RequestHeadersType& request_headers,
    int64_t ukm_download_id)
    : guid(guid),
      request_origin(request_origin),
      download_source(download_source),
      ukm_download_id(ukm_download_id),
      fetch_error_body(fetch_error_body),
      request_headers(request_headers) {}

DownloadEntry::~DownloadEntry() = default;

bool DownloadEntry::operator==(const DownloadEntry& other) const {
  return guid == other.guid && request_origin == other.request_origin &&
         download_source == other.download_source &&
         ukm_download_id == other.ukm_download_id &&
         bytes_wasted == other.bytes_wasted &&
         fetch_error_body == other.fetch_error_body &&
         request_headers == other.request_headers;
}

bool DownloadEntry::operator!=(const DownloadEntry& other) const {
  return !(*this == other);
}

}  // namespace download
