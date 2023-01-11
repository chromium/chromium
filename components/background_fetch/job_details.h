// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BACKGROUND_FETCH_JOB_DETAILS_H_
#define COMPONENTS_BACKGROUND_FETCH_JOB_DETAILS_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/background_fetch_delegate.h"

namespace content {
struct BackgroundFetchDescription;
}

namespace background_fetch {

struct JobDetails {
  enum class State {
    kPendingWillStartPaused,
    kPendingWillStartDownloading,
    kStartedButPaused,
    kStartedAndDownloading,
    // The job was aborted.
    kCancelled,
    // All requests were processed (either succeeded or failed).
    kDownloadsComplete,
    // The appropriate completion event (success, fail, abort) has been
    // dispatched.
    kJobComplete,
  };

  JobDetails();
  JobDetails(const JobDetails& other) = delete;
  JobDetails& operator=(const JobDetails& other) = delete;
  ~JobDetails();

  void MarkJobAsStarted();
  void UpdateJobOnDownloadComplete(const std::string& download_guid);

  // Gets the total number of bytes that need to be processed, or -1 if unknown.
  uint64_t GetTotalBytes() const;

  // Returns how many bytes have been processed by the Download Service so
  // far.
  uint64_t GetProcessedBytes() const;

  // Returns the number of downloaded bytes, including for the in progress
  // requests.
  uint64_t GetDownloadedBytes() const;

  // Whether the job has finished successfully (not aborted).
  bool IsComplete() const;

  void UpdateInProgressBytes(const std::string& download_guid,
                             uint64_t bytes_uploaded,
                             uint64_t bytes_downloaded);

  // Whether we should report progress of the job in terms of size of
  // downloads or in terms of the number of files being downloaded.
  bool ShouldReportProgressBySize() const;

  // Returns the number of bytes processed by in-progress requests.
  uint64_t GetInProgressBytes() const;

  struct RequestData {
    enum class Status {
      kAbsent,
      kIncluded,
    };

    explicit RequestData(bool has_upload_data);
    ~RequestData();

    Status status;

    uint64_t body_size_bytes = 0u;
    uint64_t in_progress_uploaded_bytes = 0u;
    uint64_t in_progress_downloaded_bytes = 0u;
  };

  // The client to report the Background Fetch updates to.
  base::WeakPtr<content::BackgroundFetchDelegate::Client> client;

  // Set of DownloadService GUIDs that are currently processed. They are
  // added by DownloadUrl and are removed when the fetch completes, fails,
  // or is cancelled.
  std::map<std::string, RequestData> current_fetch_guids;

  State job_state;
  std::unique_ptr<content::BackgroundFetchDescription> fetch_description;
  bool cancelled_from_ui = false;

  base::OnceClosure on_resume;
};

}  // namespace background_fetch

#endif  // COMPONENTS_BACKGROUND_FETCH_JOB_DETAILS_H_
