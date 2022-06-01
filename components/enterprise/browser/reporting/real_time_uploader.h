// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REAL_TIME_UPLOADER_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REAL_TIME_UPLOADER_H_

#include "base/memory/weak_ptr.h"
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

  // Returns true if report queue is ready.
  bool IsEnabled() const;

  // Creates the reporting::ReportQueue.
  void CreateReportQueue(const std::string& dm_token,
                         reporting::Destination destination);

  // Uploads the |report|. This API must be called after CreateReportQueue().
  // However, the caller doesn't have to wait for async queue creation. The
  // reports that are added before queue is ready will be cached and sent out
  // afterwards.
  // The |callback| will be called once the report is in the queue. There is no
  // callback for report uploading.
  virtual void Upload(std::unique_ptr<google::protobuf::MessageLite> report,
                      EnqueueCallback callback);

 protected:
  explicit RealTimeUploader(reporting::Priority priority);
  // virtual function for unit test to fake
  // reporting::ReportQueueProvider::CreateQueue() call before API providing a
  // fake implementation.
  virtual void CreateReportQueueRequest(
      reporting::StatusOr<std::unique_ptr<reporting::ReportQueueConfiguration>>
          config,
      reporting::ReportQueueProvider::CreateReportQueueCallback callback);

 private:
  void OnReportQueueCreated(
      reporting::ReportQueueProvider::CreateReportQueueResponse
          create_report_queue_response);

  void UploadClosure(
      std::unique_ptr<const google::protobuf::MessageLite> report,
      EnqueueCallback callback);
  void OnReportEnqueued(EnqueueCallback callback, reporting::Status status);

  std::unique_ptr<reporting::ReportQueue> report_queue_;

  const reporting::Priority report_priority_;

  std::queue<base::OnceClosure> pending_reports_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<RealTimeUploader> weak_factory_{this};
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REAL_TIME_UPLOADER_H_
