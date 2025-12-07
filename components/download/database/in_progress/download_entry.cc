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

}  // namespace download
