// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_GENERATOR_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_GENERATOR_H_

#include <memory>
#include <queue>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/enterprise/browser/reporting/browser_report_generator.h"
#include "components/enterprise/browser/reporting/report_request_definition.h"
#include "components/enterprise/browser/reporting/report_request_queue_generator.h"
#include "components/enterprise/browser/reporting/report_type.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace enterprise_reporting {

class ReportingDelegateFactory;

class ReportGenerator {
 public:
  using ReportRequest = definition::ReportRequest;
  using ReportRequests = std::queue<std::unique_ptr<ReportRequest>>;
  using ReportCallback = base::OnceCallback<void(ReportRequests)>;

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
  virtual ~ReportGenerator();

  // Asynchronously generates a queue of report requests, providing them to
  // |callback| when ready. If |report_type| is kFull, all details are
  // included for all loaded profiles. Otherwise, the report only contains
  // information that are needed by that particular type.
  virtual void Generate(ReportType report_type, ReportCallback callback);

  void SetMaximumReportSizeForTesting(size_t size);

 protected:
  // Creates a basic request that will be used by all Profiles.
  void CreateBasicRequest(std::unique_ptr<ReportRequest> basic_request,
                          ReportType report_type,
                          ReportCallback callback);

  // Returns an OS report contains basic OS information includes OS name, OS
  // architecture and OS version.
  virtual std::unique_ptr<enterprise_management::OSReport> GetOSReport();

  // Returns the name of computer.
  virtual std::string GetMachineName();

  // Returns the name of OS user.
  virtual std::string GetOSUserName();

  // Returns the Serial number of the device. It's Windows only field and empty
  // on other platforms.
  virtual std::string GetSerialNumber();

 private:
  void OnBrowserReportReady(
      ReportType report_type,
      ReportCallback callback,
      std::unique_ptr<ReportRequest> basic_request,
      std::unique_ptr<enterprise_management::BrowserReport> browser_report);

  std::unique_ptr<Delegate> delegate_;

  ReportRequestQueueGenerator report_request_queue_generator_;
  BrowserReportGenerator browser_report_generator_;

  base::WeakPtrFactory<ReportGenerator> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ReportGenerator);
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_GENERATOR_H_
