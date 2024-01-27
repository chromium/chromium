// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_DRIVER_ENTRY_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_DRIVER_ENTRY_H_

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"
#include "url/gurl.h"

#if BUILDFLAG(USE_BLINK)
#include "storage/browser/blob/blob_data_handle.h"
#endif

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace download {

// A snapshot of the states of a download. It's preferred to use the data on the
// fly and query new ones from download driver, instead of caching the states.
struct DriverEntry {
  // States of the download. Mostly maps to
  // download::DownloadItem::DownloadState.
  enum class State {
    IN_PROGRESS = 0,
    COMPLETE = 1,
    CANCELLED = 2,
    INTERRUPTED = 3,
    UNKNOWN = 4, /* Not created from a download item object. */
  };

  DriverEntry();
  DriverEntry(const DriverEntry& other);
  ~DriverEntry();

  // The unique identifier of the download.
  std::string guid;

  // The current state of the download.
  State state;

  // If the download is paused.
  bool paused;

  // If the download is done.
  bool done;

  // Whether the download is resumable. Determined by whether "Accept-Ranges" or
  // "Content-Range" is present in the response headers and if it has strong
  // validators. If false, the download may not be resumable.
  bool can_resume;

  // The number of bytes downloaded.
  uint64_t bytes_downloaded;

  // The expected total size of the download, set to 0 if the Content-Length
  // http header is not presented.
  uint64_t expected_total_size;

  // The physical file path for the download. It can be different from the
  // target file path requested while the file is downloading, as it may
  // download to a temporary path. After completion, this would be set to the
  // target file path.
  // Will be empty file path in incognito mode.
  base::FilePath current_file_path;

#if BUILDFLAG(USE_BLINK)
  // The blob data handle that contains download data.
  // Will be available after the download is completed in incognito mode.
  std::optional<storage::BlobDataHandle> blob_handle;
#endif

  // Time the download was marked as complete, base::Time() if the download is
  // not yet complete.
  base::Time completion_time;

  // The response headers for the most recent download request.
  scoped_refptr<const net::HttpResponseHeaders> response_headers;

  // The url chain of the download. Download may encounter redirects, and
  // fetches the content from the last url in the chain.
  std::vector<GURL> url_chain;

  // An optional base::HexEncoded SHA-256 hash (if available) of the file
  // contents.  If empty there is no available hash value.
  std::string hash256;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_DRIVER_ENTRY_H_
