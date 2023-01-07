// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/report_request.h"

#include "components/enterprise/browser/reporting/report_type.h"

namespace enterprise_reporting {

namespace em = enterprise_management;

ReportRequest::ReportRequest(ReportType type) {
  switch (type) {
    case ReportType::kFull:
    case ReportType::kBrowserVersion:
#if BUILDFLAG(IS_CHROMEOS_ASH)
      proto_.emplace<em::ChromeOsUserReportRequest>();
#else
      proto_.emplace<em::ChromeDesktopReportRequest>();
#endif
      return;
    case ReportType::kProfileReport:
      proto_.emplace<em::ChromeProfileReportRequest>();
      return;
  }
}

ReportRequest::ReportRequest(const em::ChromeDesktopReportRequest& proto)
    : proto_(proto) {}
ReportRequest::ReportRequest(const em::ChromeOsUserReportRequest& proto)
    : proto_(proto) {}
ReportRequest::ReportRequest(const em::ChromeProfileReportRequest& proto)
    : proto_(proto) {}

ReportRequest::~ReportRequest() = default;

const ReportRequest::DeviceReportRequestProto&
ReportRequest::GetDeviceReportRequest() const {
  return absl::get<ReportRequest::DeviceReportRequestProto>(proto_);
}
ReportRequest::DeviceReportRequestProto&
ReportRequest::GetDeviceReportRequest() {
  return absl::get<ReportRequest::DeviceReportRequestProto>(proto_);
}

const em::ChromeProfileReportRequest&
ReportRequest::GetChromeProfileReportRequest() const {
  return absl::get<em::ChromeProfileReportRequest>(proto_);
}
em::ChromeProfileReportRequest& ReportRequest::GetChromeProfileReportRequest() {
  return absl::get<em::ChromeProfileReportRequest>(proto_);
}

std::unique_ptr<ReportRequest> ReportRequest::Clone() const {
  return absl::visit(
      [](const auto& proto) { return std::make_unique<ReportRequest>(proto); },
      proto_);
}

}  // namespace enterprise_reporting
