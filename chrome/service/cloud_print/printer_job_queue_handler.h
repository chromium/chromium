// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_PRINTER_JOB_QUEUE_HANDLER_H_
#define CHROME_SERVICE_CLOUD_PRINT_PRINTER_JOB_QUEUE_HANDLER_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"

namespace base {
class Value;
}

namespace cloud_print {

struct JobDetails {
  static bool Ordering(const JobDetails& first, const JobDetails& second);

  JobDetails();
  JobDetails(const JobDetails& other);
  ~JobDetails();

  void Clear();

  std::string job_id_;
  std::string job_title_;
  std::string job_owner_;

  std::string print_ticket_url_;
  std::string print_data_url_;

  std::string print_ticket_;
  std::string print_ticket_mime_type_;
  base::FilePath print_data_file_path_;
  std::string print_data_mime_type_;

  std::vector<std::string> tags_;

  base::TimeDelta time_remaining_;
};

// class containing logic for print job backoff

class PrinterJobQueueHandler {
 public:
  class TimeProvider {
   public:
    virtual base::Time GetNow() = 0;
    virtual ~TimeProvider() {}
  };

  PrinterJobQueueHandler();

  PrinterJobQueueHandler(const PrinterJobQueueHandler&) = delete;
  PrinterJobQueueHandler& operator=(const PrinterJobQueueHandler&) = delete;

  ~PrinterJobQueueHandler();

  // Returns a vector with details of all jobs in the queue, sorted by time
  // until they are ready to print, lowest to highest. Jobs that are ready to
  // print will have a |time_remaining_| of 0.
  std::vector<JobDetails> GetJobsFromQueue(const base::Value& json_data);

  // Marks a job fetch as failed. Returns "true" if the job will be retried.
  bool JobFetchFailed(const std::string& job_id);

  void JobDone(const std::string& job_id);

 protected:
  // Only used for testing.
  explicit PrinterJobQueueHandler(std::unique_ptr<TimeProvider> time_provider);

  TimeProvider* time_provider() { return time_provider_.get(); }

 private:
  base::TimeDelta ComputeBackoffTime(const std::string& job_id);

  std::unique_ptr<TimeProvider> time_provider_;

  struct FailedJobMetadata {
    int retries_;
    base::Time last_retry_;
  };

  using FailedJobMap = std::map<std::string, FailedJobMetadata>;
  using FailedJobPair = std::pair<std::string, FailedJobMetadata>;

  FailedJobMap failed_job_map_;
};

}  // namespace cloud_print

#endif  // CHROME_SERVICE_CLOUD_PRINT_PRINTER_JOB_QUEUE_HANDLER_H_
