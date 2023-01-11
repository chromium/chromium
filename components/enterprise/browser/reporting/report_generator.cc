// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/report_generator.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/enterprise/browser/reporting/browser_report_generator.h"
#include "components/enterprise/browser/reporting/os_report_generator.h"
#include "components/enterprise/browser/reporting/report_type.h"
#include "components/enterprise/browser/reporting/reporting_delegate_factory.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/wmi.h"
#endif

namespace em = enterprise_management;

namespace enterprise_reporting {

ReportGenerator::ReportGenerator(ReportingDelegateFactory* delegate_factory)
    : delegate_(delegate_factory->GetReportGeneratorDelegate()),
      report_request_queue_generator_(delegate_factory),
      browser_report_generator_(delegate_factory) {}

ReportGenerator::~ReportGenerator() = default;

void ReportGenerator::Generate(ReportType report_type,
                               ReportCallback callback) {
  CreateBasicRequest(std::make_unique<ReportRequest>(report_type), report_type,
                     std::move(callback));
}

void ReportGenerator::CreateBasicRequest(
    std::unique_ptr<ReportRequest> basic_request,
    ReportType report_type,
    ReportCallback callback) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  delegate_->SetAndroidAppInfos(basic_request.get());
#else
  basic_request->GetDeviceReportRequest().set_computer_name(
      this->GetMachineName());
  basic_request->GetDeviceReportRequest().set_os_user_name(GetOSUserName());
  basic_request->GetDeviceReportRequest().set_serial_number(GetSerialNumber());
  basic_request->GetDeviceReportRequest().set_allocated_os_report(
      GetOSReport().release());
  basic_request->GetDeviceReportRequest()
      .set_allocated_browser_device_identifier(
          policy::GetBrowserDeviceIdentifier().release());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // 1. Async function base::SysInfo::SetHardwareInfo is called.
  // 2. ReportGenerator::SetHardwareInfo fills basic_report
  // 3. ReportGenerator::GenerateReport is called

  base::SysInfo::GetHardwareInfo(
      base::BindOnce(&ReportGenerator::SetHardwareInfo,
                     weak_ptr_factory_.GetWeakPtr(), std::move(basic_request),
                     base::BindOnce(&ReportGenerator::GenerateReport,
                                    weak_ptr_factory_.GetWeakPtr(), report_type,
                                    std::move(callback))));
#else
  GenerateReport(report_type, std::move(callback), std::move(basic_request));
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}

std::string ReportGenerator::GetMachineName() {
  return policy::GetMachineName();
}

std::string ReportGenerator::GetOSUserName() {
  return policy::GetOSUsername();
}

std::string ReportGenerator::GetSerialNumber() {
#if BUILDFLAG(IS_WIN)
  return base::WideToUTF8(
      base::win::WmiComputerSystemInfo::Get().serial_number());
#else
  return std::string();
#endif
}

void ReportGenerator::GenerateReport(
    ReportType report_type,
    ReportCallback callback,
    std::unique_ptr<ReportRequest> basic_request) {
  browser_report_generator_.Generate(
      ReportType::kFull,
      base::BindOnce(&ReportGenerator::OnBrowserReportReady,
                     weak_ptr_factory_.GetWeakPtr(), std::move(basic_request),
                     report_type, std::move(callback)));
}

void ReportGenerator::OnBrowserReportReady(
    std::unique_ptr<ReportRequest> basic_request,
    ReportType report_type,
    ReportCallback callback,
    std::unique_ptr<em::BrowserReport> browser_report) {
  basic_request->GetDeviceReportRequest().set_allocated_browser_report(
      browser_report.release());

  if (report_type != ReportType::kBrowserVersion) {
    // Generate a queue of requests containing detailed profile information.
    std::move(callback).Run(
        report_request_queue_generator_.Generate(*basic_request));
    return;
  }

  // Return a queue containing only the basic request and browser report without
  // detailed profile information.
  ReportRequestQueue requests;
  requests.push(std::move(basic_request));
  std::move(callback).Run(std::move(requests));
}

void ReportGenerator::SetHardwareInfo(
    std::unique_ptr<ReportRequest> basic_request,
    base::OnceCallback<void(std::unique_ptr<ReportRequest>)> callback,
    base::SysInfo::HardwareInfo hardware_info) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  basic_request->GetDeviceReportRequest().set_brand_name(
      hardware_info.manufacturer);
  basic_request->GetDeviceReportRequest().set_device_model(hardware_info.model);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

  std::move(callback).Run(std::move(basic_request));
}

}  // namespace enterprise_reporting
