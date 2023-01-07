// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REAL_TIME_UPLOADER_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REAL_TIME_UPLOADER_H_

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "components/reporting/client/report_queue_provider.h"

namespace enterprise_reporting {

// A helper class to upload real time reports with ERP. It needs to be created
// and used on the main thread.
class RealTimeUploader {
 public:
  using EnqueueCallback = base::OnceCallback<void(bool)>;

  static std::unique_ptr<RealTimeUploader> Create(
      const std::string& dm_token,
      reporting::Destination destination,
      reporting::Priority priority);

  RealTimeUploader(const RealTimeUploader&) = delete;
  RealTimeUploader& operator=(const RealTimeUploader&) = delete;
  virtual ~RealTimeUploader();

  // Uploads the |report|. This API must be called after CreateReportQueue().
  // However, the caller doesn't have to wait for async queue creation. The
  // reports that are added before queue is ready will be cached and sent out
  // afterwards.
  // The |callback| will be called once the report is in the queue. There is no
  // callback for report uploading.
  virtual void Upload(std::unique_ptr<google::protobuf::MessageLite> report,
                      EnqueueCallback callback);

  reporting::ReportQueue* GetReportQueue() const;

 protected:
  explicit RealTimeUploader(reporting::Priority priority);
  // Creates the reporting::ReportQueue.
  void CreateReportQueue(const std::string& dm_token,
                         reporting::Destination destination);

 private:
  void OnReportEnqueued(EnqueueCallback callback, reporting::Status status);

  std::unique_ptr<reporting::ReportQueue, base::OnTaskRunnerDeleter>
      report_queue_;

  const reporting::Priority report_priority_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<RealTimeUploader> weak_factory_{this};
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REAL_TIME_UPLOADER_H_
