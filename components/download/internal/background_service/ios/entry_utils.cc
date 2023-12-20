// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/ios/entry_utils.h"

#include "components/download/internal/background_service/entry.h"
#include "components/download/public/background_service/download_metadata.h"

namespace download {
namespace util {

std::map<DownloadClient, std::vector<DownloadMetaData>>
MapEntriesToMetadataForClients(const std::set<DownloadClient>& clients,
                               const std::vector<Entry*>& entries) {
  std::map<DownloadClient, std::vector<DownloadMetaData>> categorized;

  for (Entry* entry : entries) {
    DownloadClient client = entry->client;
    if (clients.find(client) == clients.end())
      continue;

    DownloadMetaData meta_data;
    meta_data.guid = entry->guid;
    // iOS currently doesn't support pause.
    meta_data.paused = false;
    // Unlike other platforms that uses history db through download driver, the
    // current size on iOS is always based on background download proto db
    // record.
    meta_data.current_size = entry->bytes_downloaded;
    if (entry->state == Entry::State::COMPLETE) {
      // TODO(xingliu): Implement the response headers and url chain with
      // NSURLSession.
      meta_data.completion_info =
          CompletionInfo(entry->target_file_path, entry->bytes_downloaded,
                         entry->url_chain, entry->response_headers);
    }

    categorized[client].emplace_back(std::move(meta_data));
  }

  return categorized;
}

}  // namespace util
}  // namespace download
