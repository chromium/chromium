// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_DATABASE_IN_PROGRESS_DOWNLOAD_ENTRY_H_
#define COMPONENTS_DOWNLOAD_DATABASE_IN_PROGRESS_DOWNLOAD_ENTRY_H_

#include <string>

#include "components/download/public/common/download_source.h"
#include "components/download/public/common/download_url_parameters.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace download {

// Contains various in-progress information related to a download.
struct DownloadEntry {
 public:
  DownloadEntry();
  DownloadEntry(const DownloadEntry& other);
  DownloadEntry(
      const std::string& guid,
      const std::string& request_origin,
      DownloadSource download_source,
      bool fetch_error_body,
      const DownloadUrlParameters::RequestHeadersType& request_headers,
      int64_t ukm_id);
  ~DownloadEntry();

  bool operator==(const DownloadEntry& other) const;

  bool operator!=(const DownloadEntry& other) const;

  // A unique GUID that represents this download.
  std::string guid;

  // Represents the origin information for this download. Used by offline pages.
  std::string request_origin;

  // The source that triggered the download.
  DownloadSource download_source = DownloadSource::UNKNOWN;

  // Unique ID that tracks the download UKM entry, where 0 means the
  // download_id is not yet initialized.
  uint64_t ukm_download_id = 0;

  // Count for how many (extra) bytes were used (including resumption).
  int64_t bytes_wasted = 0;

  // If the entity body of unsuccessful HTTP response, like HTTP 404, will be
  // downloaded.
  bool fetch_error_body = false;

  // Request header key/value pairs that will be added to the download HTTP
  // request.
  DownloadUrlParameters::RequestHeadersType request_headers;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_DATABASE_IN_PROGRESS_DOWNLOAD_ENTRY_H_
