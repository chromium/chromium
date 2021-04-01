// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REAL_TIME_UPLOADER_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REAL_TIME_UPLOADER_H_

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/reporting/client/report_queue_provider.h"

namespace enterprise_reporting {

class RealTimeUploader {
 public:
  RealTimeUploader();
  RealTimeUploader(const RealTimeUploader&) = delete;
  RealTimeUploader& operator=(const RealTimeUploader&) = delete;
  virtual ~RealTimeUploader();
  // Returns true if report queue is ready.
  bool IsEnabled();

  static std::unique_ptr<RealTimeUploader> Create(
      const std::string& dm_token,
      reporting::Destination destination);

  // Creates the reporting::ReportQueue.
  void CreateReportQueue(const std::string& dm_token,
                         reporting::Destination destination);

 protected:
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

  std::unique_ptr<reporting::ReportQueue> report_queue_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<RealTimeUploader> weak_factory_{this};
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REAL_TIME_UPLOADER_H_
