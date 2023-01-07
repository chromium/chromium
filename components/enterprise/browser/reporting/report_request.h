// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_REQUEST_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_REQUEST_H_

#include <memory>
#include <queue>

#include "build/chromeos_buildflags.h"
#include "device_management_backend.pb.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace enterprise_reporting {

enum class ReportType;

// A class is used to allow reports of multiple types to be stored in a single
// queue for the uploader.
class ReportRequest {
 public:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  using DeviceReportRequestProto =
      enterprise_management::ChromeOsUserReportRequest;
#else
  using DeviceReportRequestProto =
      enterprise_management::ChromeDesktopReportRequest;
#endif

  explicit ReportRequest(ReportType type);
  explicit ReportRequest(
      const enterprise_management::ChromeDesktopReportRequest& proto);
  explicit ReportRequest(
      const enterprise_management::ChromeOsUserReportRequest& proto);
  explicit ReportRequest(
      const enterprise_management::ChromeProfileReportRequest& proto);

  ReportRequest(const ReportRequest&) = delete;
  ReportRequest& operator=(const ReportRequest&) = delete;
  ~ReportRequest();

  const DeviceReportRequestProto& GetDeviceReportRequest() const;
  DeviceReportRequestProto& GetDeviceReportRequest();

  const enterprise_management::ChromeProfileReportRequest&
  GetChromeProfileReportRequest() const;
  enterprise_management::ChromeProfileReportRequest&
  GetChromeProfileReportRequest();

  // Clone the request proto.
  // This is required when a report is splitted into multiple requests.
  // As all requests need to contain shared information like browser version or
  // machine name. Long term, we can improves the splitting algorithm so Clone
  // is no longer necessary.
  std::unique_ptr<ReportRequest> Clone() const;

 private:
  absl::variant<enterprise_management::ChromeDesktopReportRequest,
                enterprise_management::ChromeOsUserReportRequest,
                enterprise_management::ChromeProfileReportRequest>
      proto_;
};

using ReportRequestQueue = std::queue<std::unique_ptr<ReportRequest>>;

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_REQUEST_H_
