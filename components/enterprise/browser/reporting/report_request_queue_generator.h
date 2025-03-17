// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_REQUEST_QUEUE_GENERATOR_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_REQUEST_QUEUE_GENERATOR_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/enterprise/browser/reporting/profile_report_generator.h"
#include "components/enterprise/browser/reporting/report_request.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace enterprise_reporting {

class ReportingDelegateFactory;

// Generate a report request queue that contains full profile information. The
// request number in the queue is decided by the maximum report size setting.
// TODO(crbug.com/40704763): Unit tests for this class are still in
// chrome/browser/enterprise/reporting.
class ReportRequestQueueGenerator {
 public:
  explicit ReportRequestQueueGenerator(
      ReportingDelegateFactory* delegate_factory);
  ReportRequestQueueGenerator(const ReportRequestQueueGenerator&) = delete;
  ReportRequestQueueGenerator& operator=(const ReportRequestQueueGenerator&) =
      delete;
  ~ReportRequestQueueGenerator();

  // Get the maximum report size.
  size_t GetMaximumReportSizeForTesting() const;

  // Set the maximum report size. The full profile info will be skipped or moved
  // to another new request if its size exceeds the limit.
  void SetMaximumReportSizeForTesting(size_t maximum_report_size);

  // Generate a queue of requests including full profile info based on given
  // basic request. Will invoke `callback` with the queue when done.
  void Generate(std::unique_ptr<ReportRequest> basic_request,
                base::OnceCallback<void(ReportRequestQueue)> callback);

 private:
  using IndexedProfileReport =
      std::pair<int,
                std::unique_ptr<enterprise_management::ChromeUserProfileInfo>>;

  // Generate request with full profile info at `profile_index` according to
  // `basic_request`. Will invoke `callback` when done.
  void GenerateProfileReportWithIndex(
      int profile_index,
      const ReportRequest* basic_request,
      base::OnceCallback<void(IndexedProfileReport)> callback);

  // Called when all Profile reports were generated. `basic_request` is carried
  // over to ensure it remains alive while individual reports are being
  // generated.
  void OnAllProfileReportsGenerated(
      std::unique_ptr<ReportRequest> basic_request,
      base::OnceCallback<void(ReportRequestQueue)> callback,
      std::vector<IndexedProfileReport> indexed_reports);

 private:
  size_t maximum_report_size_;
  ProfileReportGenerator profile_report_generator_;

  base::WeakPtrFactory<ReportRequestQueueGenerator> weak_factory_{this};
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_REQUEST_QUEUE_GENERATOR_H_
