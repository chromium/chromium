// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REAL_TIME_REPORT_CONTROLLER_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REAL_TIME_REPORT_CONTROLLER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/enterprise/browser/reporting/real_time_report_generator.h"
#include "components/enterprise/browser/reporting/real_time_report_type.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/reporting/proto/synced/record_constants.pb.h"

namespace enterprise_reporting {

class RealTimeUploader;
class ReportingDelegateFactory;

class RealTimeReportController {
 public:
  explicit RealTimeReportController(ReportingDelegateFactory* delegate_factory);
  RealTimeReportController(const RealTimeReportController&) = delete;
  RealTimeReportController& operator=(const RealTimeReportController&) = delete;
  ~RealTimeReportController();

  using TriggerCallback =
      base::RepeatingCallback<void(RealTimeReportType,
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

  struct ReportConfig {
    RealTimeReportType type;
    reporting::Destination destination;
    reporting::Priority priority;
  };

  void OnDMTokenUpdated(policy::DMToken&& dm_token);

  void GenerateAndUploadReport(RealTimeReportType type,
                               const RealTimeReportGenerator::Data& data);

  void SetUploaderForTesting(RealTimeReportType type,
                             std::unique_ptr<RealTimeUploader> uploader);
  void SetReportGeneratorForTesting(
      std::unique_ptr<RealTimeReportGenerator> generator);
  Delegate* GetDelegateForTesting();

 private:
  // Creates and uploads a report with real time reporting pipeline.
  void UploadReport(const RealTimeReportGenerator::Data& data,
                    const ReportConfig& config);

  policy::DMToken dm_token_ = policy::DMToken::CreateEmptyToken();
  std::unique_ptr<RealTimeReportGenerator> real_time_report_generator_;

  base::flat_map<RealTimeReportType, std::unique_ptr<RealTimeUploader>>
      report_uploaders_;
  std::unique_ptr<Delegate> delegate_;

  base::WeakPtrFactory<RealTimeReportController> weak_ptr_factory_{this};
};
}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REAL_TIME_REPORT_CONTROLLER_H_
