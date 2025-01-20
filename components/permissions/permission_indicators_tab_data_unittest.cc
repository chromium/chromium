// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_indicators_tab_data.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/permissions/permission_uma_util.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_web_contents_factory.h"

namespace permissions {

class PermissionIndicatorsTabDataTest : public testing::Test {
 public:
  PermissionIndicatorsTabDataTest()
      : web_contents_(web_contents_factory_.CreateWebContents(&context_)) {
    ResetHistograms();
  }

 protected:
  void SetUp() override {
    tab_data_ = std::make_unique<PermissionIndicatorsTabData>(web_contents_);
  }

  PermissionIndicatorsTabData& tab_data() { return *tab_data_; }

  void ResetHistograms() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  const base::HistogramTester& histogram_tester() { return *histogram_tester_; }

  content::WebContents* web_contents() { return web_contents_; }

  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::TestBrowserContext context_;
  std::unique_ptr<PermissionIndicatorsTabData> tab_data_;
  content::TestWebContentsFactory web_contents_factory_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  raw_ptr<content::WebContents> web_contents_;
};

TEST_F(PermissionIndicatorsTabDataTest, GeolocationUsageRecord) {
  GURL url1("https://www.google.com");
  GURL url2("https://www.example.com");

  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url1);
  // UMA will be recorded if used = true;
  // The data will be recorded only when a new record coming in or the instance
  // is being destroyed.
  tab_data().OnCapabilityTypesChanged(
      content::WebContents::CapabilityType::kGeolocation, true);
  tab_data().OnCapabilityTypesChanged(
      content::WebContents::CapabilityType::kGeolocation, false);
  task_environment()->AdvanceClock(base::Seconds(5));
  tab_data().OnCapabilityTypesChanged(
      content::WebContents::CapabilityType::kGeolocation, true);
  // UMA won't be recorded if used = false.
  tab_data().OnCapabilityTypesChanged(
      content::WebContents::CapabilityType::kGeolocation, false);
  task_environment()->AdvanceClock(base::Seconds(11));
  tab_data().OnCapabilityTypesChanged(
      content::WebContents::CapabilityType::kGeolocation, true);
  tab_data().OnCapabilityTypesChanged(
      content::WebContents::CapabilityType::kGeolocation, false);
  task_environment()->AdvanceClock(base::Minutes(2));
  tab_data().OnCapabilityTypesChanged(
      content::WebContents::CapabilityType::kGeolocation, true);
  tab_data().OnCapabilityTypesChanged(
      content::WebContents::CapabilityType::kGeolocation, false);
  task_environment()->AdvanceClock(base::Minutes(6));
  tab_data().OnCapabilityTypesChanged(
      content::WebContents::CapabilityType::kGeolocation, true);
  tab_data().OnCapabilityTypesChanged(
      content::WebContents::CapabilityType::kGeolocation, false);
  task_environment()->AdvanceClock(base::Minutes(11));
  tab_data().OnCapabilityTypesChanged(
      content::WebContents::CapabilityType::kGeolocation, true);
  tab_data().OnCapabilityTypesChanged(
      content::WebContents::CapabilityType::kGeolocation, false);
  task_environment()->AdvanceClock(base::Hours(2));
  tab_data().OnCapabilityTypesChanged(
      content::WebContents::CapabilityType::kGeolocation, true);
  tab_data().OnCapabilityTypesChanged(
      content::WebContents::CapabilityType::kGeolocation, false);
  task_environment()->AdvanceClock(base::Hours(17));
  tab_data().OnCapabilityTypesChanged(
      content::WebContents::CapabilityType::kGeolocation, true);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url2);
  histogram_tester().ExpectTotalCount(
      "Permissions.Usage.ElapsedTimeSinceLastUsage.Geolocation", 7);
  histogram_tester().ExpectTimeBucketCount(
      "Permissions.Usage.ElapsedTimeSinceLastUsage.Geolocation",
      base::Seconds(5), 1);
  histogram_tester().ExpectTimeBucketCount(
      "Permissions.Usage.ElapsedTimeSinceLastUsage.Geolocation",
      base::Seconds(11), 1);
  histogram_tester().ExpectTimeBucketCount(
      "Permissions.Usage.ElapsedTimeSinceLastUsage.Geolocation",
      base::Minutes(2), 1);
  histogram_tester().ExpectTimeBucketCount(
      "Permissions.Usage.ElapsedTimeSinceLastUsage.Geolocation",
      base::Minutes(6), 1);
  histogram_tester().ExpectTimeBucketCount(
      "Permissions.Usage.ElapsedTimeSinceLastUsage.Geolocation",
      base::Minutes(11), 1);
  // Overflow bucket:
  histogram_tester().ExpectTimeBucketCount(
      "Permissions.Usage.ElapsedTimeSinceLastUsage.Geolocation",
      base::Hours(2), 2);
}

// The following tested:
// 1. 00:00:00 start location usage.
// 2. 00:10:00 stop location usage.
// 3. 00:10:05 start location usage.
// 4. 00:15:05 stop location usage.
// 5. 00:15:10 start location usage.
// 6. 00:25:10 stop location usage.
TEST_F(PermissionIndicatorsTabDataTest, GeolocationWatchUsageRecord) {
  GURL url1("https://www.google.com");
  GURL url2("https://www.example.com");

  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url1);
  tab_data().OnCapabilityTypesChanged(
      content::WebContents::CapabilityType::kGeolocation, true);
  task_environment()->AdvanceClock(base::Minutes(10));
  tab_data().OnCapabilityTypesChanged(
      content::WebContents::CapabilityType::kGeolocation, false);
  task_environment()->AdvanceClock(base::Seconds(5));
  tab_data().OnCapabilityTypesChanged(
      content::WebContents::CapabilityType::kGeolocation, true);
  task_environment()->AdvanceClock(base::Minutes(5));
  tab_data().OnCapabilityTypesChanged(
      content::WebContents::CapabilityType::kGeolocation, false);
  task_environment()->AdvanceClock(base::Seconds(5));
  tab_data().OnCapabilityTypesChanged(
      content::WebContents::CapabilityType::kGeolocation, true);
  task_environment()->AdvanceClock(base::Minutes(10));
  tab_data().OnCapabilityTypesChanged(
      content::WebContents::CapabilityType::kGeolocation, false);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url2);
  histogram_tester().ExpectTotalCount(
      "Permissions.Usage.ElapsedTimeSinceLastUsage.Geolocation", 2);
  histogram_tester().ExpectTimeBucketCount(
      "Permissions.Usage.ElapsedTimeSinceLastUsage.Geolocation",
      base::Seconds(5), 2);
}

TEST_F(PermissionIndicatorsTabDataTest, GeolocationUsageRecordAfterNavigation) {
  GURL url1("https://www.google.com");
  GURL url2("https://www.example.com");

  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url1);
  tab_data().OnCapabilityTypesChanged(
      content::WebContents::CapabilityType::kGeolocation, true);
  task_environment()->AdvanceClock(base::Seconds(11));
  tab_data().OnCapabilityTypesChanged(
      content::WebContents::CapabilityType::kGeolocation, true);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url2);
  tab_data().OnCapabilityTypesChanged(
      content::WebContents::CapabilityType::kGeolocation, true);
  tab_data().OnCapabilityTypesChanged(
      content::WebContents::CapabilityType::kGeolocation, false);
  task_environment()->AdvanceClock(base::Seconds(5));
  tab_data().OnCapabilityTypesChanged(
      content::WebContents::CapabilityType::kGeolocation, true);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url1);
  tab_data().OnCapabilityTypesChanged(
      content::WebContents::CapabilityType::kGeolocation, true);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url2);
  histogram_tester().ExpectTotalCount(
      "Permissions.Usage.ElapsedTimeSinceLastUsage.Geolocation", 1);
  histogram_tester().ExpectTimeBucketCount(
      "Permissions.Usage.ElapsedTimeSinceLastUsage.Geolocation",
      base::Seconds(5), 1);
}

TEST_F(PermissionIndicatorsTabDataTest, MicrophoneUsageRecord) {
  GURL url1("https://www.google.com");
  GURL url2("https://www.example.com");

  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url1);
  tab_data().OnMediaCaptureChanged(
      RequestTypeForUma::PERMISSION_MEDIASTREAM_MIC, true);
  tab_data().OnMediaCaptureChanged(
      RequestTypeForUma::PERMISSION_MEDIASTREAM_MIC, false);
  task_environment()->AdvanceClock(base::Seconds(11));
  tab_data().OnMediaCaptureChanged(
      RequestTypeForUma::PERMISSION_MEDIASTREAM_MIC, true);
  tab_data().OnMediaCaptureChanged(
      RequestTypeForUma::PERMISSION_MEDIASTREAM_MIC, false);
  task_environment()->AdvanceClock(base::Minutes(2));
  tab_data().OnMediaCaptureChanged(
      RequestTypeForUma::PERMISSION_MEDIASTREAM_MIC, true);
  tab_data().OnMediaCaptureChanged(
      RequestTypeForUma::PERMISSION_MEDIASTREAM_MIC, false);
  task_environment()->AdvanceClock(base::Minutes(6));
  tab_data().OnMediaCaptureChanged(
      RequestTypeForUma::PERMISSION_MEDIASTREAM_MIC, true);
  tab_data().OnMediaCaptureChanged(
      RequestTypeForUma::PERMISSION_MEDIASTREAM_MIC, false);
  task_environment()->AdvanceClock(base::Minutes(11));
  tab_data().OnMediaCaptureChanged(
      RequestTypeForUma::PERMISSION_MEDIASTREAM_MIC, true);
  tab_data().OnMediaCaptureChanged(
      RequestTypeForUma::PERMISSION_MEDIASTREAM_MIC, false);
  task_environment()->AdvanceClock(base::Hours(2));
  tab_data().OnMediaCaptureChanged(
      RequestTypeForUma::PERMISSION_MEDIASTREAM_MIC, true);
  tab_data().OnMediaCaptureChanged(
      RequestTypeForUma::PERMISSION_MEDIASTREAM_MIC, false);
  task_environment()->AdvanceClock(base::Hours(17));
  tab_data().OnMediaCaptureChanged(
      RequestTypeForUma::PERMISSION_MEDIASTREAM_MIC, true);
  tab_data().OnMediaCaptureChanged(
      RequestTypeForUma::PERMISSION_MEDIASTREAM_MIC, false);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url2);
  histogram_tester().ExpectTotalCount(
      "Permissions.Usage.ElapsedTimeSinceLastUsage.AudioCapture", 6);
  histogram_tester().ExpectTimeBucketCount(
      "Permissions.Usage.ElapsedTimeSinceLastUsage.AudioCapture",
      base::Seconds(11), 1);
  histogram_tester().ExpectTimeBucketCount(
      "Permissions.Usage.ElapsedTimeSinceLastUsage.AudioCapture",
      base::Minutes(2), 1);
  histogram_tester().ExpectTimeBucketCount(
      "Permissions.Usage.ElapsedTimeSinceLastUsage.AudioCapture",
      base::Minutes(6), 1);
  histogram_tester().ExpectTimeBucketCount(
      "Permissions.Usage.ElapsedTimeSinceLastUsage.AudioCapture",
      base::Minutes(11), 1);
  // Overflow bucket:
  histogram_tester().ExpectTimeBucketCount(
      "Permissions.Usage.ElapsedTimeSinceLastUsage.AudioCapture",
      base::Hours(2), 2);
}

TEST_F(PermissionIndicatorsTabDataTest, CameraUsageRecord) {
  GURL url1("https://www.google.com");
  GURL url2("https://www.example.com");

  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url1);
  tab_data().OnMediaCaptureChanged(
      RequestTypeForUma::PERMISSION_MEDIASTREAM_CAMERA, true);
  tab_data().OnMediaCaptureChanged(
      RequestTypeForUma::PERMISSION_MEDIASTREAM_CAMERA, false);
  task_environment()->AdvanceClock(base::Seconds(11));
  tab_data().OnMediaCaptureChanged(
      RequestTypeForUma::PERMISSION_MEDIASTREAM_CAMERA, true);
  tab_data().OnMediaCaptureChanged(
      RequestTypeForUma::PERMISSION_MEDIASTREAM_CAMERA, false);
  task_environment()->AdvanceClock(base::Minutes(2));
  tab_data().OnMediaCaptureChanged(
      RequestTypeForUma::PERMISSION_MEDIASTREAM_CAMERA, true);
  tab_data().OnMediaCaptureChanged(
      RequestTypeForUma::PERMISSION_MEDIASTREAM_CAMERA, false);
  task_environment()->AdvanceClock(base::Minutes(6));
  tab_data().OnMediaCaptureChanged(
      RequestTypeForUma::PERMISSION_MEDIASTREAM_CAMERA, true);
  tab_data().OnMediaCaptureChanged(
      RequestTypeForUma::PERMISSION_MEDIASTREAM_CAMERA, false);
  task_environment()->AdvanceClock(base::Minutes(11));
  tab_data().OnMediaCaptureChanged(
      RequestTypeForUma::PERMISSION_MEDIASTREAM_CAMERA, true);
  tab_data().OnMediaCaptureChanged(
      RequestTypeForUma::PERMISSION_MEDIASTREAM_CAMERA, false);
  task_environment()->AdvanceClock(base::Hours(2));
  tab_data().OnMediaCaptureChanged(
      RequestTypeForUma::PERMISSION_MEDIASTREAM_CAMERA, true);
  tab_data().OnMediaCaptureChanged(
      RequestTypeForUma::PERMISSION_MEDIASTREAM_CAMERA, false);
  task_environment()->AdvanceClock(base::Hours(17));
  tab_data().OnMediaCaptureChanged(
      RequestTypeForUma::PERMISSION_MEDIASTREAM_CAMERA, true);
  tab_data().OnMediaCaptureChanged(
      RequestTypeForUma::PERMISSION_MEDIASTREAM_CAMERA, false);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url2);
  histogram_tester().ExpectTotalCount(
      "Permissions.Usage.ElapsedTimeSinceLastUsage.VideoCapture", 6);
  histogram_tester().ExpectTimeBucketCount(
      "Permissions.Usage.ElapsedTimeSinceLastUsage.VideoCapture",
      base::Seconds(11), 1);
  histogram_tester().ExpectTimeBucketCount(
      "Permissions.Usage.ElapsedTimeSinceLastUsage.VideoCapture",
      base::Minutes(2), 1);
  histogram_tester().ExpectTimeBucketCount(
      "Permissions.Usage.ElapsedTimeSinceLastUsage.VideoCapture",
      base::Minutes(6), 1);
  histogram_tester().ExpectTimeBucketCount(
      "Permissions.Usage.ElapsedTimeSinceLastUsage.VideoCapture",
      base::Minutes(11), 1);
  // Overflow bucket:
  histogram_tester().ExpectTimeBucketCount(
      "Permissions.Usage.ElapsedTimeSinceLastUsage.VideoCapture",
      base::Hours(2), 2);
}

}  // namespace permissions
