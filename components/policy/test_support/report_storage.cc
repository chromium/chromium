// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/report_storage.h"

#include <utility>

namespace policy {

ReportStorage::ReportStorage() = default;

ReportStorage::~ReportStorage() = default;

void ReportStorage::AddBrowserReport(
    std::string device_id,
    enterprise_management::ChromeDesktopReportRequest report,
    base::Time timestamp) {
  base::AutoLock lock(lock_);
  browser_reports_.push_back(
      {std::move(device_id), std::move(report), timestamp});
}

std::vector<ReportStorage::BrowserReportEntry>
ReportStorage::GetBrowserReports() const {
  base::AutoLock lock(lock_);
  return browser_reports_;
}

int ReportStorage::GetBrowserReportCount() const {
  base::AutoLock lock(lock_);
  return browser_reports_.size();
}

void ReportStorage::AddProfileReport(
    std::string device_id,
    enterprise_management::ChromeProfileReportRequest report,
    base::Time timestamp) {
  base::AutoLock lock(lock_);
  profile_reports_.push_back(
      {std::move(device_id), std::move(report), timestamp});
}

std::vector<ReportStorage::ProfileReportEntry>
ReportStorage::GetProfileReports() const {
  base::AutoLock lock(lock_);
  return profile_reports_;
}

int ReportStorage::GetProfileReportCount() const {
  base::AutoLock lock(lock_);
  return profile_reports_.size();
}

void ReportStorage::AddRealtimeEvents(
    const ::chrome::cros::reporting::proto::UploadEventsRequest& request) {
  base::AutoLock lock(lock_);
  realtime_reports_.push_back(request);
}

std::vector<::chrome::cros::reporting::proto::UploadEventsRequest>
ReportStorage::GetRealtimeReports() const {
  base::AutoLock lock(lock_);
  return realtime_reports_;
}

std::vector<::chrome::cros::reporting::proto::Event>
ReportStorage::GetRealtimeEvents() const {
  base::AutoLock lock(lock_);
  std::vector<::chrome::cros::reporting::proto::Event> events;
  for (const auto& report : realtime_reports_) {
    events.insert(events.end(), report.events().begin(), report.events().end());
  }
  return events;
}

int ReportStorage::GetRealtimeReportCount() const {
  base::AutoLock lock(lock_);
  return realtime_reports_.size();
}

}  // namespace policy
