// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/report_request_queue_generator.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "build/chromeos_buildflags.h"
#include "components/enterprise/browser/reporting/report_type.h"

namespace enterprise_reporting {

namespace {

const size_t kMaximumReportSize =
    5000000;  // The report size limitation is 5mb.

constexpr char kRequestCountMetricsName[] =
    "Enterprise.CloudReportingRequestCount";
constexpr char kRequestSizeMetricsName[] =
    "Enterprise.CloudReportingRequestSize";
constexpr char kBasicRequestSizeMetricsName[] =
    "Enterprise.CloudReportingBasicRequestSize";

// Because server only stores 20 profiles for each report and when report is
// separated into requests, there is at least one profile per request. It means
// server will truncate the report when there are more than 20 requests. Actions
// are needed if there are many reports exceed this limitation.
const int kRequestCountMetricMaxValue = 21;

}  // namespace

ReportRequestQueueGenerator::ReportRequestQueueGenerator(
    ReportingDelegateFactory* delegate_factory)
    : maximum_report_size_(kMaximumReportSize),
      profile_report_generator_(delegate_factory) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // For Chrome OS, policy information needn't be uploaded to DM server.
  profile_report_generator_.set_policies_enabled(false);
#endif
}

ReportRequestQueueGenerator::~ReportRequestQueueGenerator() = default;

size_t ReportRequestQueueGenerator::GetMaximumReportSizeForTesting() const {
  return maximum_report_size_;
}

void ReportRequestQueueGenerator::SetMaximumReportSizeForTesting(
    size_t maximum_report_size) {
  maximum_report_size_ = maximum_report_size;
}

ReportRequestQueue ReportRequestQueueGenerator::Generate(
    const ReportRequest& basic_request) {
  ReportRequestQueue requests;
  size_t basic_request_size =
      basic_request.GetDeviceReportRequest().ByteSizeLong();
  base::UmaHistogramMemoryKB(kBasicRequestSizeMetricsName,
                             basic_request_size / 1024);

  if (basic_request_size <= maximum_report_size_) {
    requests.push(basic_request.Clone());
    int profile_infos_size = basic_request.GetDeviceReportRequest()
                                 .browser_report()
                                 .chrome_user_profile_infos_size();
    for (int index = 0; index < profile_infos_size; index++) {
      GenerateProfileReportWithIndex(index, basic_request, &requests);
    }

    base::UmaHistogramMemoryKB(
        kRequestSizeMetricsName,
        requests.back()->GetDeviceReportRequest().ByteSizeLong() / 1024);
  }

  base::UmaHistogramExactLinear(kRequestCountMetricsName, requests.size(),
                                kRequestCountMetricMaxValue);
  return requests;
}

void ReportRequestQueueGenerator::GenerateProfileReportWithIndex(
    int profile_index,
    const ReportRequest& basic_request,
    ReportRequestQueue* requests) {
  DCHECK_LT(profile_index, basic_request.GetDeviceReportRequest()
                               .browser_report()
                               .chrome_user_profile_infos_size());

  size_t basic_request_size =
      basic_request.GetDeviceReportRequest().ByteSizeLong();
  auto basic_profile = basic_request.GetDeviceReportRequest()
                           .browser_report()
                           .chrome_user_profile_infos(profile_index);
  auto profile_report = profile_report_generator_.MaybeGenerate(
      base::FilePath::FromUTF8Unsafe(basic_profile.id()), basic_profile.name(),
      ReportType::kFull);

  // Return if Profile is not loaded and there is no full report.
  if (!profile_report)
    return;

  // Use size diff to calculate estimated request size after full profile report
  // is added. There are still few bytes difference but close enough.
  size_t profile_report_incremental_size =
      profile_report->ByteSizeLong() - basic_profile.ByteSizeLong();
  size_t current_request_size =
      requests->back()->GetDeviceReportRequest().ByteSizeLong();

  if (current_request_size + profile_report_incremental_size <=
      maximum_report_size_) {
    // The new full Profile report can be appended into the current request.
    requests->back()
        ->GetDeviceReportRequest()
        .mutable_browser_report()
        ->mutable_chrome_user_profile_infos(profile_index)
        ->Swap(profile_report.get());
  } else if (basic_request_size + profile_report_incremental_size <=
             maximum_report_size_) {
    // The new full Profile report is too big to be appended into the current
    // request, move it to the next request if possible. Record metrics for the
    // current request's size.
    base::UmaHistogramMemoryKB(
        kRequestSizeMetricsName,
        requests->back()->GetDeviceReportRequest().ByteSizeLong() / 1024);
    requests->push(basic_request.Clone());
    requests->back()
        ->GetDeviceReportRequest()
        .mutable_browser_report()
        ->mutable_chrome_user_profile_infos(profile_index)
        ->Swap(profile_report.get());
  } else {
    // The new full Profile report is too big to be uploaded, skip this
    // Profile report. But we still add the report size into metrics so
    // that we could understand the situation better.
    base::UmaHistogramMemoryKB(
        kRequestSizeMetricsName,
        (basic_request_size + profile_report_incremental_size) / 1024);
  }
}

}  // namespace enterprise_reporting
