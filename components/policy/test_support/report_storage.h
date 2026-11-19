// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_REPORT_STORAGE_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_REPORT_STORAGE_H_

#include <string>
#include <vector>

#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "components/enterprise/common/proto/upload_request_response.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

// Thread-safe storage for Chrome browser/profile reports and realtime security
// event reports received by the test server.
class ReportStorage {
 public:
  template <typename Report>
  struct ReportEntry {
    std::string device_id;
    Report report;
    base::Time timestamp;
  };
  using BrowserReportEntry =
      ReportEntry<enterprise_management::ChromeDesktopReportRequest>;
  using ProfileReportEntry =
      ReportEntry<enterprise_management::ChromeProfileReportRequest>;

  ReportStorage();
  ReportStorage(const ReportStorage&) = delete;
  ReportStorage& operator=(const ReportStorage&) = delete;
  ~ReportStorage();

  void AddBrowserReport(
      std::string device_id,
      enterprise_management::ChromeDesktopReportRequest report,
      base::Time timestamp = base::Time::Now());
  std::vector<BrowserReportEntry> GetBrowserReports() const;
  int GetBrowserReportCount() const;

  void AddProfileReport(
      std::string device_id,
      enterprise_management::ChromeProfileReportRequest report,
      base::Time timestamp = base::Time::Now());
  std::vector<ProfileReportEntry> GetProfileReports() const;
  int GetProfileReportCount() const;

  void AddRealtimeEvents(
      const ::chrome::cros::reporting::proto::UploadEventsRequest& request);
  std::vector<::chrome::cros::reporting::proto::UploadEventsRequest>
  GetRealtimeReports() const;
  std::vector<::chrome::cros::reporting::proto::Event> GetRealtimeEvents()
      const;
  int GetRealtimeReportCount() const;

 private:
  mutable base::Lock lock_;
  std::vector<BrowserReportEntry> browser_reports_ GUARDED_BY(lock_);
  std::vector<ProfileReportEntry> profile_reports_ GUARDED_BY(lock_);
  std::vector<::chrome::cros::reporting::proto::UploadEventsRequest>
      realtime_reports_ GUARDED_BY(lock_);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_REPORT_STORAGE_H_
