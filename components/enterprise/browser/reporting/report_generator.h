// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_GENERATOR_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_GENERATOR_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/system/sys_info.h"
#include "components/enterprise/browser/reporting/browser_report_generator.h"
#include "components/enterprise/browser/reporting/report_request.h"
#include "components/enterprise/browser/reporting/report_request_queue_generator.h"
#include "components/enterprise/browser/reporting/report_type.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace enterprise_reporting {

class ReportingDelegateFactory;

class ReportGenerator {
 public:
  using ReportCallback = base::OnceCallback<void(ReportRequestQueue)>;

  class Delegate {
   public:
    Delegate() = default;
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    virtual ~Delegate() = default;

    // Collect the Android application information installed on primary profile,
    // and set it to |basic_request_|. Only implemented for Chrome OS. The
    // fields are empty on other platforms.
    virtual void SetAndroidAppInfos(ReportRequest* basic_request) = 0;
  };

  explicit ReportGenerator(ReportingDelegateFactory* delegate_factory);

  ReportGenerator(const ReportGenerator&) = delete;
  ReportGenerator& operator=(const ReportGenerator&) = delete;

  virtual ~ReportGenerator();

  // Asynchronously generates a queue of report requests, providing them to
  // |callback| when ready. If |report_type| is kFull, all details are
  // included for all loaded profiles. Otherwise, the report only contains
  // information that are needed by that particular type.
  virtual void Generate(ReportType report_type, ReportCallback callback);

 protected:
  // Creates a basic request that will be used by all Profiles.
  void CreateBasicRequest(std::unique_ptr<ReportRequest> basic_request,
                          ReportType report_type,
                          ReportCallback callback);

  // Returns the name of computer.
  virtual std::string GetMachineName();

  // Returns the name of OS user.
  virtual std::string GetOSUserName();

  // Returns the Serial number of the device. It's Windows only field and empty
  // on other platforms.
  virtual std::string GetSerialNumber();

 private:
  void GenerateReport(ReportType report_type,
                      ReportCallback callback,
                      std::unique_ptr<ReportRequest> basic_request);

  void SetHardwareInfo(
      std::unique_ptr<ReportRequest> basic_request,
      base::OnceCallback<void(std::unique_ptr<ReportRequest>)> callback,
      base::SysInfo::HardwareInfo hardware_info);

  void OnBrowserReportReady(
      std::unique_ptr<ReportRequest> basic_request,
      ReportType report_type,
      ReportCallback callback,
      std::unique_ptr<enterprise_management::BrowserReport> browser_report);

  std::unique_ptr<Delegate> delegate_;

  ReportRequestQueueGenerator report_request_queue_generator_;
  BrowserReportGenerator browser_report_generator_;

  base::WeakPtrFactory<ReportGenerator> weak_ptr_factory_{this};
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_GENERATOR_H_
