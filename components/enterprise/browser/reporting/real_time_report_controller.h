// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REAL_TIME_REPORT_CONTROLLER_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REAL_TIME_REPORT_CONTROLLER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/enterprise/browser/reporting/real_time_report_generator.h"
#include "components/policy/core/common/cloud/dm_token.h"

namespace enterprise_reporting {

class RealTimeUploader;
class ReportingDelegateFactory;

class RealTimeReportController {
 public:
  explicit RealTimeReportController(ReportingDelegateFactory* delegate_factory);
  RealTimeReportController(const RealTimeReportController&) = delete;
  RealTimeReportController& operator=(const RealTimeReportController&) = delete;
  ~RealTimeReportController();

  enum ReportTrigger {
    kExtensionRequest,
  };

  using TriggerCallback =
      base::RepeatingCallback<void(ReportTrigger,
                                   const RealTimeReportGenerator::Data&)>;

  class Delegate {
   public:
    Delegate();
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    virtual ~Delegate();

    void SetTriggerCallback(TriggerCallback callback);

    // Extension request
    virtual void StartWatchingExtensionRequestIfNeeded() = 0;
    virtual void StopWatchingExtensionRequest() = 0;

   protected:
    TriggerCallback trigger_callback_;
  };

  void OnDMTokenUpdated(policy::DMToken&& dm_token);

  void GenerateAndUploadReport(ReportTrigger trigger,
                               const RealTimeReportGenerator::Data& data);

  void SetExtensionRequestUploaderForTesting(
      std::unique_ptr<RealTimeUploader> uploader);
  void SetReportGeneratorForTesting(
      std::unique_ptr<RealTimeReportGenerator> generator);
  Delegate* GetDelegateForTesting();

 private:
  // Creates and uploads extension requests with real time reporting pipeline.
  void UploadExtensionRequests(const RealTimeReportGenerator::Data& data);

  policy::DMToken dm_token_ = policy::DMToken::CreateEmptyToken();
  std::unique_ptr<RealTimeUploader> extension_request_uploader_;
  std::unique_ptr<RealTimeReportGenerator> real_time_report_generator_;

  std::unique_ptr<Delegate> delegate_;

  base::WeakPtrFactory<RealTimeReportController> weak_ptr_factory_{this};
};
}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REAL_TIME_REPORT_CONTROLLER_H_
