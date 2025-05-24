// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_CHROME_PROFILE_REQUEST_GENERATOR_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_CHROME_PROFILE_REQUEST_GENERATOR_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/enterprise/browser/reporting/browser_report_generator.h"
#include "components/enterprise/browser/reporting/profile_report_generator.h"
#include "components/enterprise/browser/reporting/report_generation_config.h"
#include "components/enterprise/browser/reporting/report_request.h"

namespace device_signals {
class SignalsAggregator;
}

namespace enterprise_reporting {

class ReportingDelegateFactory;

// The top level generator that creates ChromeProfileRequest proto.
class ChromeProfileRequestGenerator {
 public:
  using ReportCallback = base::OnceCallback<void(ReportRequestQueue)>;

  ChromeProfileRequestGenerator(
      const base::FilePath& profile_path,
      ReportingDelegateFactory* delegate_factory,
      device_signals::SignalsAggregator* signals_aggregator = nullptr);
  ChromeProfileRequestGenerator(const ChromeProfileRequestGenerator&) = delete;
  ChromeProfileRequestGenerator& operator=(
      const ChromeProfileRequestGenerator&) = delete;

  virtual ~ChromeProfileRequestGenerator();

  virtual void Generate(ReportGenerationConfig generation_config,
                        ReportCallback callback);

  void ToggleExtensionReport(
      ProfileReportGenerator::ExtensionsEnabledCallback callback);

 private:
  void OnBaseReportsReady(
      std::unique_ptr<ReportRequest> request,
      ReportCallback callback,
      ReportGenerationConfig generation_config,
      std::unique_ptr<enterprise_management::BrowserReport> browser_report,
      std::unique_ptr<enterprise_management::ChromeUserProfileInfo>
          profile_report);

  void OnAggregatedSignalsReceived(
      std::unique_ptr<ReportRequest> request,
      ReportCallback callback,
      std::unique_ptr<enterprise_management::BrowserReport> browser_report,
      std::unique_ptr<enterprise_management::ChromeUserProfileInfo>
          profile_report,
      device_signals::SignalsAggregationResponse response);

  void OnRequestReady(std::unique_ptr<ReportRequest> request,
                      ReportCallback callback);

  const base::FilePath profile_path_;

  BrowserReportGenerator browser_report_generator_;
  ProfileReportGenerator profile_report_generator_;

  raw_ptr<device_signals::SignalsAggregator> signals_aggregator_;

  base::WeakPtrFactory<ChromeProfileRequestGenerator> weak_ptr_factory_{this};
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_CHROME_PROFILE_REQUEST_GENERATOR_H_
