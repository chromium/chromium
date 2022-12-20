// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/database/in_progress/in_progress_info.h"

namespace download {

InProgressInfo::InProgressInfo() = default;

InProgressInfo::InProgressInfo(const InProgressInfo& other) = default;

InProgressInfo::~InProgressInfo() = default;

bool InProgressInfo::operator==(const InProgressInfo& other) const {
  return url_chain == other.url_chain &&
         serialized_embedder_download_data ==
             other.serialized_embedder_download_data &&
         referrer_url == other.referrer_url && tab_url == other.tab_url &&
         tab_referrer_url == other.tab_referrer_url &&
         fetch_error_body == other.fetch_error_body &&
         request_headers == other.request_headers && etag == other.etag &&
         last_modified == other.last_modified &&
         total_bytes == other.total_bytes && mime_type == other.mime_type &&
         original_mime_type == other.original_mime_type &&
         current_path == other.current_path &&
         target_path == other.target_path &&
         received_bytes == other.received_bytes &&
         start_time == other.start_time && end_time == other.end_time &&
         received_slices == other.received_slices && hash == other.hash &&
         transient == other.transient && state == other.state &&
         danger_type == other.danger_type &&
         interrupt_reason == other.interrupt_reason && paused == other.paused &&
         metered == other.metered && bytes_wasted == other.bytes_wasted &&
         auto_resume_count == other.auto_resume_count &&
         credentials_mode == other.credentials_mode &&
         range_request_from == other.range_request_from &&
         range_request_to == other.range_request_to;
}

}  // namespace download
