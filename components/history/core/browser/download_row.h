// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_DOWNLOAD_ROW_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_DOWNLOAD_ROW_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "components/history/core/browser/download_constants.h"
#include "components/history/core/browser/download_slice_info.h"
#include "components/history/core/browser/download_types.h"
#include "url/gurl.h"

namespace history {

// Contains the information that is stored in the download system's persistent
// store (or refers to it). DownloadHistory uses this to communicate with the
// DownloadDatabase through the HistoryService.
struct DownloadRow {
  DownloadRow();
  DownloadRow(const DownloadRow& other);
  DownloadRow(DownloadRow&& other);
  ~DownloadRow();

  DownloadRow& operator=(const DownloadRow&);
  bool operator==(const DownloadRow&) const;

  // The current path to the download (potentially different from final if
  // download is in progress or interrupted).
  base::FilePath current_path;

  // The target path where the download will go when it's complete.
  base::FilePath target_path;

  // The URL redirect chain through which we are downloading.  The front
  // is the url that the initial request went to, and the back is the
  // url from which we ended up getting data.  This is not changed by
  // UpdateDownload().
  std::vector<GURL> url_chain;

  // The URL that referred us. Is not changed by UpdateDownload().
  GURL referrer_url;

  // The site URL for the site instance that initiated the download.
  GURL site_url;

  // The serialized proto for embedder-specific data that pertains to the site
  // instance that initiated the download. The embedder is responsible for
  // serializing and deserializing this data.
  std::string embedder_download_data;

  // The URL of the tab that initiated the download, if any. Not changed by
  // UpdateDownload().
  GURL tab_url;

  // The referrer of the tab that initialized the download, if any. Not changed
  // by UpdateDownload();
  GURL tab_referrer_url;

  // HTTP method used for the request. GET is assumed if the method was not
  // stored for a download in the history database.
  std::string http_method;

  // The MIME type of the download, might be based on heuristics.
  std::string mime_type;

  // The original MIME type of the download.
  std::string original_mime_type;

  // The time when the download started. Is not changed by UpdateDownload().
  base::Time start_time;

  // The time when the download completed.
  base::Time end_time;

  // Contents of most recently seen ETag header.
  std::string etag;

  // Contents of most recently seen Last-Modified header.
  std::string last_modified;

  // The number of bytes received (so far).
  int64_t received_bytes = 0;

  // The total number of bytes in the download. Is not changed by
  // UpdateDownload().
  int64_t total_bytes = 0;

  // The current state of the download.
  DownloadState state = DownloadState::IN_PROGRESS;

  // Whether and how the download is dangerous.
  DownloadDangerType danger_type = DownloadDangerType::NOT_DANGEROUS;

  // The reason the download was interrupted, if state == kStateInterrupted.
  DownloadInterruptReason interrupt_reason = 0;

  // The raw SHA-256 hash of the complete or partial download contents. Not hex
  // encoded.
  std::string hash;

  // The id of the download in the database. Is not changed by UpdateDownload().
  // Note: This field should be considered deprecated in favor of `guid` below.
  // See http://crbug.com/593020.
  DownloadId id = kInvalidDownloadId;

  // The GUID of the download in the database. Not changed by UpdateDownload().
  std::string guid;

  // Whether this download has ever been opened from the browser.
  bool opened = false;

  // The time when the download was last accessed.
  base::Time last_access_time;

  // Whether this download is transient. Transient items are cleaned up after
  // completion and not shown in the UI.
  bool transient = false;

  // The id and name of the extension that created this download.
  std::string by_ext_id;
  std::string by_ext_name;

  // The id of the web app that created this download.
  std::string by_web_app_id;

  // Data slices that have been downloaded so far. The slices must be ordered
  // by their offset.
  std::vector<DownloadSliceInfo> download_slice_info;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_DOWNLOAD_ROW_H_
