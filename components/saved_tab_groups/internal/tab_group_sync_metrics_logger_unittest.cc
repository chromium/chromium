// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/saved_tab_groups/internal/tab_group_sync_metrics_logger_impl.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Eq;
using testing::Invoke;
using testing::Return;
using UkmSavedTabGroupNavigation = ukm::builders::SavedTabGroup_Navigation;

namespace tab_groups {
namespace {

// Remote device guids.
const char kDeviceGuid1[] = "device1";
const char kDeviceGuid2[] = "device2";

// Local device guid.
const char kDeviceGuid3[] = "device3";

const char kTestURL1[] = "http://foo.com/test.html?x=1#123";
const char kTestURL2[] = "http://foo.com/test.html#12";
const char kTestURL3[] = "http://foo.com/test2.html#12";
}  // namespace

class TabGroupSyncMetricsLoggerTest : public testing::Test {
 public:
  TabGroupSyncMetricsLoggerTest() = default;

  ~TabGroupSyncMetricsLoggerTest() override = default;

  void SetUp() override {
    device_info_tracker_ = std::make_unique<syncer::FakeDeviceInfoTracker>();
    metrics_logger_ = std::make_unique<TabGroupSyncMetricsLoggerImpl>(
        device_info_tracker_.get());
    test_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    metrics_logger_->SetClockForTesting(&test_clock_);
    SetUpDeviceInfo();
  }

  void SetUpDeviceInfo() {
    device_info1_ = test::CreateDeviceInfo(
        kDeviceGuid1, syncer::DeviceInfo::OsType::kAndroid,
        syncer::DeviceInfo::FormFactor::kPhone);
    device_info2_ = test::CreateDeviceInfo(
        kDeviceGuid2, syncer::DeviceInfo::OsType::kWindows,
        syncer::DeviceInfo::FormFactor::kDesktop);
    device_info3_ = test::CreateDeviceInfo(
        kDeviceGuid3, syncer::DeviceInfo::OsType::kAndroid,
        syncer::DeviceInfo::FormFactor::kTablet);
    device_info_tracker_->Add(device_info1_.get());
    device_info_tracker_->Add(device_info2_.get());
    device_info_tracker_->Add(device_info3_.get());
    device_info_tracker_->SetLocalCacheGuid(kDeviceGuid3);
  }

  DeviceType GetDeviceType(syncer::DeviceInfo::OsType os_type,
                           syncer::DeviceInfo::FormFactor form_factor) {
    auto device_info = test::CreateDeviceInfo("test", os_type, form_factor);
    return metrics_logger_->GetDeviceTypeFromDeviceInfo(*device_info);
  }

  struct ExpectedValue {
    int expected_num_same_url_load;
    int expected_num_same_url_without_ref_and_query_param_load;
    bool is_post;
    bool was_redirected;
    SavedTabGroupType type;

    ExpectedValue(int expected_num_same_url_load,
                  int expected_num_same_url_without_ref_and_query_param_load,
                  bool is_post,
                  bool was_redirected,
                  SavedTabGroupType type)
        : expected_num_same_url_load(expected_num_same_url_load),
          expected_num_same_url_without_ref_and_query_param_load(
              expected_num_same_url_without_ref_and_query_param_load),
          is_post(is_post),
          was_redirected(was_redirected),
          type(type) {}
  };

  void ExpectUkmMetrics(std::vector<ExpectedValue> expected_values) {
    const auto& entries = test_recorder_->GetEntriesByName(
        UkmSavedTabGroupNavigation::kEntryName);
    EXPECT_EQ(expected_values.size(), entries.size());
    for (size_t i = 0; i < entries.size(); ++i) {
      const ukm::mojom::UkmEntry* entry = entries[i];
      const ExpectedValue& value = expected_values[i];
      test_recorder_->ExpectEntryMetric(
          entry, UkmSavedTabGroupNavigation::kNumSameUrlLoadName,
          value.expected_num_same_url_load);
      test_recorder_->ExpectEntryMetric(
          entry,
          UkmSavedTabGroupNavigation::
              kNumSameUrlWithoutRefAndQueryParamLoadName,
          value.expected_num_same_url_without_ref_and_query_param_load);
      test_recorder_->ExpectEntryMetric(entry,
                                        UkmSavedTabGroupNavigation::kIsPostName,
                                        static_cast<int64_t>(value.is_post));
      test_recorder_->ExpectEntryMetric(
          entry, UkmSavedTabGroupNavigation::kWasRedirectedName,
          static_cast<int64_t>(value.was_redirected));
      test_recorder_->ExpectEntryMetric(
          entry, UkmSavedTabGroupNavigation::kTabGroupTypeName,
          static_cast<int64_t>(value.type));
    }
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<syncer::FakeDeviceInfoTracker> device_info_tracker_;
  std::unique_ptr<TabGroupSyncMetricsLoggerImpl> metrics_logger_;
  std::unique_ptr<syncer::DeviceInfo> device_info1_;
  std::unique_ptr<syncer::DeviceInfo> device_info2_;
  std::unique_ptr<syncer::DeviceInfo> device_info3_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_recorder_;
  base::SimpleTestClock test_clock_;
};

TEST_F(TabGroupSyncMetricsLoggerTest, HistogramsAreEmittedForLogEvents) {
  SavedTabGroup group = test::CreateTestSavedTabGroupWithNoTabs();
  group.SetLocalGroupId(test::GenerateRandomTabGroupID());
  group.SetCreatorCacheGuid(kDeviceGuid1);

  SavedTabGroupTab tab =
      test::CreateSavedTabGroupTab("url", u"title", group.saved_guid());
  tab.SetLocalTabID(test::GenerateRandomTabID());
  tab.SetCreatorCacheGuid(kDeviceGuid2);

  // Group events.
  // Note, group open and close events are tested in separate tests.
  {
    base::HistogramTester histogram_tester;
    metrics_logger_->LogEvent(EventDetails(TabGroupEvent::kTabGroupCreated),
                              &group, &tab);
    histogram_tester.ExpectUniqueSample(
        "TabGroups.Sync.TabGroup.Created.GroupCreateOrigin",
        DeviceType::kAndroidPhone, 1u);
  }

  {
    base::HistogramTester histogram_tester;
    metrics_logger_->LogEvent(EventDetails(TabGroupEvent::kTabGroupRemoved),
                              &group, &tab);
    histogram_tester.ExpectUniqueSample(
        "TabGroups.Sync.TabGroup.Removed.GroupCreateOrigin",
        DeviceType::kAndroidPhone, 1u);
  }

  {
    base::HistogramTester histogram_tester;

    metrics_logger_->LogEvent(
        EventDetails(TabGroupEvent::kTabGroupVisualsChanged), &group, &tab);
    histogram_tester.ExpectUniqueSample(
        "TabGroups.Sync.TabGroup.VisualsChanged.GroupCreateOrigin",
        DeviceType::kAndroidPhone, 1u);
  }

  {
    base::HistogramTester histogram_tester;

    metrics_logger_->LogEvent(
        EventDetails(TabGroupEvent::kTabGroupTabsReordered), &group, &tab);
    histogram_tester.ExpectUniqueSample(
        "TabGroups.Sync.TabGroup.TabsReordered.GroupCreateOrigin",
        DeviceType::kAndroidPhone, 1u);
  }

  // Tab events.
  {
    base::HistogramTester histogram_tester;
    metrics_logger_->LogEvent(EventDetails(TabGroupEvent::kTabAdded), &group,
                              &tab);
    histogram_tester.ExpectUniqueSample(
        "TabGroups.Sync.TabGroup.TabAdded.GroupCreateOrigin",
        DeviceType::kAndroidPhone, 1u);
    histogram_tester.ExpectUniqueSample(
        "TabGroups.Sync.TabGroup.UserInteracted.GroupCreateOrigin",
        DeviceType::kAndroidPhone, 1u);
    histogram_tester.ExpectUniqueSample(
        "TabGroups.Sync.TabGroup.UserInteracted.HasTitle", true, 1u);
  }

  {
    base::HistogramTester histogram_tester;
    metrics_logger_->LogEvent(EventDetails(TabGroupEvent::kTabRemoved), &group,
                              &tab);
    histogram_tester.ExpectUniqueSample(
        "TabGroups.Sync.TabGroup.TabRemoved.GroupCreateOrigin",
        DeviceType::kAndroidPhone, 1u);
    histogram_tester.ExpectUniqueSample(
        "TabGroups.Sync.TabGroup.TabRemoved.TabCreateOrigin",
        DeviceType::kWindows, 1u);
    histogram_tester.ExpectUniqueSample(
        "TabGroups.Sync.TabGroup.UserInteracted.GroupCreateOrigin",
        DeviceType::kAndroidPhone, 1u);
    histogram_tester.ExpectUniqueSample(
        "TabGroups.Sync.TabGroup.UserInteracted.HasTitle", true, 1u);
  }

  {
    base::HistogramTester histogram_tester;
    metrics_logger_->LogEvent(EventDetails(TabGroupEvent::kTabNavigated),
                              &group, &tab);
    histogram_tester.ExpectUniqueSample(
        "TabGroups.Sync.TabGroup.TabNavigated.GroupCreateOrigin",
        DeviceType::kAndroidPhone, 1u);
    histogram_tester.ExpectUniqueSample(
        "TabGroups.Sync.TabGroup.TabNavigated.TabCreateOrigin",
        DeviceType::kWindows, 1u);
  }

  {
    base::HistogramTester histogram_tester;
    metrics_logger_->LogEvent(EventDetails(TabGroupEvent::kTabSelected), &group,
                              &tab);
    histogram_tester.ExpectUniqueSample(
        "TabGroups.Sync.TabGroup.TabSelected.GroupCreateOrigin",
        DeviceType::kAndroidPhone, 1u);
    histogram_tester.ExpectUniqueSample(
        "TabGroups.Sync.TabGroup.TabSelected.TabCreateOrigin",
        DeviceType::kWindows, 1u);
    histogram_tester.ExpectUniqueSample(
        "TabGroups.Sync.TabGroup.UserInteracted.GroupCreateOrigin",
        DeviceType::kAndroidPhone, 1u);
    histogram_tester.ExpectUniqueSample(
        "TabGroups.Sync.TabGroup.UserInteracted.HasTitle", true, 1u);
  }
}

TEST_F(TabGroupSyncMetricsLoggerTest, SomeEventsForLocalDeviceOrigin) {
  SavedTabGroup group = test::CreateTestSavedTabGroupWithNoTabs();
  group.SetLocalGroupId(test::GenerateRandomTabGroupID());
  group.SetCreatorCacheGuid(kDeviceGuid3);

  SavedTabGroupTab tab =
      test::CreateSavedTabGroupTab("url", u"title", group.saved_guid());
  tab.SetLocalTabID(test::GenerateRandomTabID());
  tab.SetCreatorCacheGuid(kDeviceGuid2);

  {
    base::HistogramTester histogram_tester;
    group.SetCreatorCacheGuid(kDeviceGuid3);
    tab.SetCreatorCacheGuid(kDeviceGuid2);
    metrics_logger_->LogEvent(EventDetails(TabGroupEvent::kTabNavigated),
                              &group, &tab);
    histogram_tester.ExpectUniqueSample(
        "TabGroups.Sync.TabGroup.TabNavigated.GroupCreateOrigin",
        DeviceType::kLocal, 1u);
    histogram_tester.ExpectUniqueSample(
        "TabGroups.Sync.TabGroup.TabNavigated.TabCreateOrigin",
        DeviceType::kWindows, 1u);
  }

  {
    base::HistogramTester histogram_tester;
    group.SetCreatorCacheGuid(kDeviceGuid2);
    tab.SetCreatorCacheGuid(kDeviceGuid3);
    metrics_logger_->LogEvent(EventDetails(TabGroupEvent::kTabNavigated),
                              &group, &tab);
    histogram_tester.ExpectUniqueSample(
        "TabGroups.Sync.TabGroup.TabNavigated.GroupCreateOrigin",
        DeviceType::kWindows, 1u);
    histogram_tester.ExpectUniqueSample(
        "TabGroups.Sync.TabGroup.TabNavigated.TabCreateOrigin",
        DeviceType::kLocal, 1u);
  }
}

TEST_F(TabGroupSyncMetricsLoggerTest, TabGroupOpenedFromRevisitUi) {
  base::HistogramTester histogram_tester;

  SavedTabGroup group = test::CreateTestSavedTabGroupWithNoTabs();
  group.SetLocalGroupId(test::GenerateRandomTabGroupID());
  group.SetCreatorCacheGuid(kDeviceGuid1);

  EventDetails details(TabGroupEvent::kTabGroupOpened);
  details.local_tab_group_id = group.local_group_id();
  details.opening_source = OpeningSource::kOpenedFromRevisitUi;

  metrics_logger_->LogEvent(details, &group, nullptr);
  histogram_tester.ExpectUniqueSample(
      "TabGroups.Sync.TabGroup.Opened.GroupCreateOrigin",
      DeviceType::kAndroidPhone, 1u);

  histogram_tester.ExpectUniqueSample("TabGroups.Sync.TabGroup.Opened.Reason",
                                      OpeningSource::kOpenedFromRevisitUi, 1u);
  histogram_tester.ExpectUniqueSample(
      "TabGroups.Sync.GroupOpenedByUser.HasTitle", true, 1u);
}

TEST_F(TabGroupSyncMetricsLoggerTest, TabGroupOpenedFromSync) {
  base::HistogramTester histogram_tester;

  SavedTabGroup group = test::CreateTestSavedTabGroupWithNoTabs();
  group.SetLocalGroupId(test::GenerateRandomTabGroupID());
  group.SetCreatorCacheGuid(kDeviceGuid1);

  EventDetails details(TabGroupEvent::kTabGroupOpened);
  details.local_tab_group_id = group.local_group_id();
  details.opening_source = OpeningSource::kAutoOpenedFromSync;

  metrics_logger_->LogEvent(details, &group, nullptr);
  histogram_tester.ExpectUniqueSample("TabGroups.Sync.TabGroup.Opened.Reason",
                                      OpeningSource::kAutoOpenedFromSync, 1u);
  // These histograms aren't recorded for events from sync.
  histogram_tester.ExpectTotalCount(
      "TabGroups.Sync.TabGroup.Opened.GroupCreateOrigin", 0u);
  histogram_tester.ExpectTotalCount("TabGroups.Sync.GroupOpenedByUser.HasTitle",
                                    0u);
}

TEST_F(TabGroupSyncMetricsLoggerTest, TabGroupClosedByUser) {
  base::HistogramTester histogram_tester;

  SavedTabGroup group = test::CreateTestSavedTabGroupWithNoTabs();
  group.SetLocalGroupId(test::GenerateRandomTabGroupID());
  group.SetCreatorCacheGuid(kDeviceGuid1);

  EventDetails details(TabGroupEvent::kTabGroupClosed);
  details.local_tab_group_id = group.local_group_id();
  details.closing_source = ClosingSource::kClosedByUser;

  metrics_logger_->LogEvent(details, &group, nullptr);
  histogram_tester.ExpectUniqueSample(
      "TabGroups.Sync.TabGroup.Closed.GroupCreateOrigin",
      DeviceType::kAndroidPhone, 1u);

  histogram_tester.ExpectUniqueSample("TabGroups.Sync.TabGroup.Closed.Reason",
                                      ClosingSource::kClosedByUser, 1u);
  histogram_tester.ExpectUniqueSample(
      "TabGroups.Sync.GroupClosedByUser.HasTitle", true, 1u);
}

TEST_F(TabGroupSyncMetricsLoggerTest, SyncGroupDeletionIsIgnoredByMetrics) {
  base::HistogramTester histogram_tester;

  SavedTabGroup group = test::CreateTestSavedTabGroupWithNoTabs();
  group.SetLocalGroupId(test::GenerateRandomTabGroupID());
  group.SetCreatorCacheGuid(kDeviceGuid1);

  EventDetails details(TabGroupEvent::kTabGroupClosed);
  details.local_tab_group_id = group.local_group_id();
  details.closing_source = ClosingSource::kDeletedFromSync;

  metrics_logger_->LogEvent(details, &group, nullptr);
  histogram_tester.ExpectUniqueSample("TabGroups.Sync.TabGroup.Closed.Reason",
                                      ClosingSource::kDeletedFromSync, 1u);

  // These histograms aren't recorded for events from sync.
  histogram_tester.ExpectTotalCount(
      "TabGroups.Sync.TabGroup.Closed.GroupCreateOrigin", 0u);
  histogram_tester.ExpectTotalCount("TabGroups.Sync.GroupClosedByUser.HasTitle",
                                    0u);
}

TEST_F(TabGroupSyncMetricsLoggerTest, DeviceTypeConversion) {
  EXPECT_EQ(DeviceType::kAndroidPhone,
            GetDeviceType(syncer::DeviceInfo::OsType::kAndroid,
                          syncer::DeviceInfo::FormFactor::kPhone));
  EXPECT_EQ(DeviceType::kAndroidTablet,
            GetDeviceType(syncer::DeviceInfo::OsType::kAndroid,
                          syncer::DeviceInfo::FormFactor::kTablet));
  EXPECT_EQ(DeviceType::kIOSPhone,
            GetDeviceType(syncer::DeviceInfo::OsType::kIOS,
                          syncer::DeviceInfo::FormFactor::kPhone));
  EXPECT_EQ(DeviceType::kIOSTablet,
            GetDeviceType(syncer::DeviceInfo::OsType::kIOS,
                          syncer::DeviceInfo::FormFactor::kTablet));

  // Unknown / unsupported form factors in Android / IOS.
  EXPECT_EQ(DeviceType::kUnknown,
            GetDeviceType(syncer::DeviceInfo::OsType::kAndroid,
                          syncer::DeviceInfo::FormFactor::kUnknown));
  EXPECT_EQ(DeviceType::kUnknown,
            GetDeviceType(syncer::DeviceInfo::OsType::kAndroid,
                          syncer::DeviceInfo::FormFactor::kDesktop));
  EXPECT_EQ(DeviceType::kUnknown,
            GetDeviceType(syncer::DeviceInfo::OsType::kIOS,
                          syncer::DeviceInfo::FormFactor::kUnknown));
  EXPECT_EQ(DeviceType::kUnknown,
            GetDeviceType(syncer::DeviceInfo::OsType::kIOS,
                          syncer::DeviceInfo::FormFactor::kDesktop));

  // Unknown OS types.
  EXPECT_EQ(DeviceType::kUnknown,
            GetDeviceType(syncer::DeviceInfo::OsType::kUnknown,
                          syncer::DeviceInfo::FormFactor::kPhone));
  EXPECT_EQ(DeviceType::kUnknown,
            GetDeviceType(syncer::DeviceInfo::OsType::kUnknown,
                          syncer::DeviceInfo::FormFactor::kTablet));
  EXPECT_EQ(DeviceType::kUnknown,
            GetDeviceType(syncer::DeviceInfo::OsType::kUnknown,
                          syncer::DeviceInfo::FormFactor::kDesktop));
  EXPECT_EQ(DeviceType::kUnknown,
            GetDeviceType(syncer::DeviceInfo::OsType::kUnknown,
                          syncer::DeviceInfo::FormFactor::kUnknown));

  // Desktop OS types.
  EXPECT_EQ(DeviceType::kWindows,
            GetDeviceType(syncer::DeviceInfo::OsType::kWindows,
                          syncer::DeviceInfo::FormFactor::kDesktop));
  EXPECT_EQ(DeviceType::kMac,
            GetDeviceType(syncer::DeviceInfo::OsType::kMac,
                          syncer::DeviceInfo::FormFactor::kDesktop));
  EXPECT_EQ(DeviceType::kLinux,
            GetDeviceType(syncer::DeviceInfo::OsType::kLinux,
                          syncer::DeviceInfo::FormFactor::kDesktop));
  EXPECT_EQ(DeviceType::kChromeOS,
            GetDeviceType(syncer::DeviceInfo::OsType::kChromeOsAsh,
                          syncer::DeviceInfo::FormFactor::kDesktop));
  EXPECT_EQ(DeviceType::kChromeOS,
            GetDeviceType(syncer::DeviceInfo::OsType::kChromeOsLacros,
                          syncer::DeviceInfo::FormFactor::kDesktop));

  // Unsupported desktop OS form factors.
  EXPECT_EQ(DeviceType::kWindows,
            GetDeviceType(syncer::DeviceInfo::OsType::kWindows,
                          syncer::DeviceInfo::FormFactor::kPhone));
  EXPECT_EQ(DeviceType::kLinux,
            GetDeviceType(syncer::DeviceInfo::OsType::kLinux,
                          syncer::DeviceInfo::FormFactor::kTablet));
  EXPECT_EQ(DeviceType::kChromeOS,
            GetDeviceType(syncer::DeviceInfo::OsType::kChromeOsAsh,
                          syncer::DeviceInfo::FormFactor::kPhone));
  EXPECT_EQ(DeviceType::kChromeOS,
            GetDeviceType(syncer::DeviceInfo::OsType::kChromeOsLacros,
                          syncer::DeviceInfo::FormFactor::kTablet));
}

TEST_F(TabGroupSyncMetricsLoggerTest, RecordMetricsOnStartup) {
  std::vector<SavedTabGroup> saved_tab_groups;
  std::vector<bool> is_remote;

  // Group 1: 1 tab, open, local, active 0 day ago.
  // Group 2: 2 tabs, closed, remote, active 15 days ago.
  SavedTabGroup group1 = test::CreateTestSavedTabGroupWithNoTabs();
  group1.SetLocalGroupId(test::GenerateRandomTabGroupID());
  SavedTabGroupTab tab1 =
      test::CreateSavedTabGroupTab("url", u"title", group1.saved_guid());
  tab1.SetLocalTabID(test::GenerateRandomTabID());
  group1.AddTabLocally(tab1);
  group1.SetLastUserInteractionTime(base::Time::Now() - base::Hours(2));
  saved_tab_groups.emplace_back(group1);
  is_remote.emplace_back(false);

  SavedTabGroup group2 = test::CreateTestSavedTabGroupWithNoTabs();
  SavedTabGroupTab tab2 =
      test::CreateSavedTabGroupTab("url", u"title", group2.saved_guid());
  SavedTabGroupTab tab3 =
      test::CreateSavedTabGroupTab("url", u"title", group2.saved_guid());
  group2.AddTabLocally(tab2);
  group2.AddTabLocally(tab3);
  group2.SetLastUserInteractionTime(base::Time::Now() - base::Days(15));
  saved_tab_groups.emplace_back(group2);
  is_remote.emplace_back(true);

  base::HistogramTester histogram_tester;
  metrics_logger_->RecordMetricsOnStartup(saved_tab_groups, is_remote);

  // Group counts.
  histogram_tester.ExpectUniqueSample("TabGroups.Sync.TotalTabGroupCount", 2,
                                      1u);
  histogram_tester.ExpectUniqueSample("TabGroups.Sync.OpenTabGroupCount", 1,
                                      1u);
  histogram_tester.ExpectUniqueSample("TabGroups.Sync.ClosedTabGroupCount", 1,
                                      1u);
  histogram_tester.ExpectUniqueSample("TabGroups.Sync.RemoteTabGroupCount", 1,
                                      1u);

  // Active tab group counts.
  histogram_tester.ExpectUniqueSample("TabGroups.Sync.ActiveTabGroupCount.1Day",
                                      1, 1u);
  histogram_tester.ExpectUniqueSample("TabGroups.Sync.ActiveTabGroupCount.7Day",
                                      1, 1u);
  histogram_tester.ExpectUniqueSample(
      "TabGroups.Sync.ActiveTabGroupCount.28Day", 2, 1u);

  histogram_tester.ExpectUniqueSample(
      "TabGroups.Sync.RemoteActiveTabGroupCount.1Day", 0, 1u);
  histogram_tester.ExpectUniqueSample(
      "TabGroups.Sync.RemoteActiveTabGroupCount.7Day", 0, 1u);
  histogram_tester.ExpectUniqueSample(
      "TabGroups.Sync.RemoteActiveTabGroupCount.28Day", 1, 1u);

  // Tab metrics.
  EXPECT_EQ(
      3u, histogram_tester.GetTotalSum("TabGroups.Sync.SavedTabGroupTabCount"));
  histogram_tester.ExpectUniqueSample("TabGroups.Sync.SavedTabGroupAge", 0, 2u);
}

TEST_F(TabGroupSyncMetricsLoggerTest, RecordSavedTabGroupNavigation) {
  LocalTabID id_1 = test::GenerateRandomTabID();
  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  metrics_logger_->RecordSavedTabGroupNavigation(
      id_1, GURL(kTestURL1), SavedTabGroupType::SYNCED,
      /*is_post=*/true,
      /*was_redirected=*/false, source_id);
  std::vector<ExpectedValue> expected_values;
  expected_values.emplace_back(1, 1, true, false, SavedTabGroupType::SYNCED);
  ExpectUkmMetrics(expected_values);

  // Navigate to a similar URL quickly.
  test_clock_.Advance(base::Minutes(1));
  metrics_logger_->RecordSavedTabGroupNavigation(
      id_1, GURL(kTestURL2), SavedTabGroupType::SYNCED,
      /*is_post=*/true,
      /*was_redirected=*/true, source_id);
  expected_values.emplace_back(1, 2, true, true, SavedTabGroupType::SYNCED);
  ExpectUkmMetrics(expected_values);

  // Navigate again, and the first URL will no longer be counted.
  test_clock_.Advance(base::Minutes(2));
  metrics_logger_->RecordSavedTabGroupNavigation(
      id_1, GURL(kTestURL2), SavedTabGroupType::SYNCED,
      /*is_post=*/true,
      /*was_redirected=*/true, source_id);
  expected_values.emplace_back(2, 2, true, true, SavedTabGroupType::SYNCED);
  ExpectUkmMetrics(expected_values);

  // Navigate in a different tab.
  LocalTabID id_2 = test::GenerateRandomTabID();
  metrics_logger_->RecordSavedTabGroupNavigation(
      id_2, GURL(kTestURL2), SavedTabGroupType::SYNCED,
      /*is_post=*/false,
      /*was_redirected=*/true, source_id);
  expected_values.emplace_back(1, 1, false, true, SavedTabGroupType::SYNCED);
  ExpectUkmMetrics(expected_values);

  // Navigate to a different page.
  metrics_logger_->RecordSavedTabGroupNavigation(
      id_2, GURL(kTestURL3), SavedTabGroupType::SYNCED,
      /*is_post=*/false,
      /*was_redirected=*/true, source_id);
  expected_values.emplace_back(1, 1, false, true, SavedTabGroupType::SYNCED);
  ExpectUkmMetrics(expected_values);

  // Wait for all past navigations to expire.
  test_clock_.Advance(base::Minutes(3));
  metrics_logger_->RecordSavedTabGroupNavigation(
      id_2, GURL(kTestURL3), SavedTabGroupType::SYNCED,
      /*is_post=*/false,
      /*was_redirected=*/true, source_id);
  expected_values.emplace_back(1, 1, false, true, SavedTabGroupType::SYNCED);
  ExpectUkmMetrics(expected_values);
}

}  // namespace tab_groups
