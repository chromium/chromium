// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/background_fetch/job_details.h"

#include "content/public/browser/background_fetch_description.h"

namespace background_fetch {

JobDetails::RequestData::RequestData(bool has_upload_data)
    : status(has_upload_data ? Status::kIncluded : Status::kAbsent) {}

JobDetails::RequestData::~RequestData() = default;

JobDetails::JobDetails() = default;
JobDetails::~JobDetails() = default;

void JobDetails::MarkJobAsStarted() {
  if (job_state == State::kPendingWillStartDownloading) {
    job_state = State::kStartedAndDownloading;
  } else if (job_state == State::kPendingWillStartPaused) {
    job_state = State::kStartedButPaused;
  }
}

void JobDetails::UpdateJobOnDownloadComplete(const std::string& download_guid) {
  fetch_description->completed_requests++;
  if (fetch_description->completed_requests ==
      fetch_description->total_requests) {
    job_state = State::kDownloadsComplete;
  }

  current_fetch_guids.erase(download_guid);
}

uint64_t JobDetails::GetTotalBytes() const {
  if (!ShouldReportProgressBySize()) {
    return -1;
  }

  // If we have completed all downloads, update progress max to the processed
  // bytes in case the provided totals were set too high. This avoids
  // unnecessary jumping in the progress bar.
  uint64_t completed_bytes =
      fetch_description->downloaded_bytes + fetch_description->uploaded_bytes;
  uint64_t total_bytes = fetch_description->download_total_bytes +
                         fetch_description->upload_total_bytes;
  return job_state == State::kDownloadsComplete ? completed_bytes : total_bytes;
}

uint64_t JobDetails::GetProcessedBytes() const {
  return fetch_description->downloaded_bytes +
         fetch_description->uploaded_bytes + GetInProgressBytes();
}

uint64_t JobDetails::GetDownloadedBytes() const {
  uint64_t bytes = fetch_description->downloaded_bytes;
  for (const auto& current_fetch : current_fetch_guids)
    bytes += current_fetch.second.in_progress_downloaded_bytes;
  return bytes;
}

uint64_t JobDetails::GetInProgressBytes() const {
  uint64_t bytes = 0u;
  for (const auto& current_fetch : current_fetch_guids) {
    bytes += current_fetch.second.in_progress_downloaded_bytes +
             current_fetch.second.in_progress_uploaded_bytes;
  }
  return bytes;
}

bool JobDetails::IsComplete() const {
  return job_state == State::kJobComplete;
}

void JobDetails::UpdateInProgressBytes(const std::string& download_guid,
                                       uint64_t bytes_uploaded,
                                       uint64_t bytes_downloaded) {
  DCHECK(current_fetch_guids.count(download_guid));
  auto& request_data = current_fetch_guids.find(download_guid)->second;

  // If we started receiving download bytes then the upload was complete and is
  // accounted for in |uploaded_bytes|.
  if (bytes_downloaded > 0u) {
    request_data.in_progress_uploaded_bytes = 0u;
  } else {
    request_data.in_progress_uploaded_bytes = bytes_uploaded;
  }

  request_data.in_progress_downloaded_bytes = bytes_downloaded;
}

bool JobDetails::ShouldReportProgressBySize() const {
  if (!fetch_description->download_total_bytes) {
    // |download_total_bytes| was not set. Cannot report by size.
    return false;
  }

  if (fetch_description->completed_requests <
          fetch_description->total_requests &&
      GetDownloadedBytes() > fetch_description->download_total_bytes) {
    // |download_total_bytes| was set too low.
    return false;
  }

  return true;
}

}  // namespace background_fetch
