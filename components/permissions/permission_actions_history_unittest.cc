// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_actions_history.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "base/containers/adapters.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "base/time/time.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/permission_request_enums.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/permissions_client.h"
#include "components/permissions/pref_names.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/test_permissions_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace permissions {
namespace {

constexpr int kHeuristicGrantThreshold = 3;

struct TestEntry {
  PermissionAction action;
  RequestType type;
  PermissionPromptDisposition prompt_disposition;
  bool advance_clock;
  bool is_legacy_entry;
} kTestEntries[]{
    {PermissionAction::DENIED, RequestType::kNotifications,
     PermissionPromptDisposition::NOT_APPLICABLE, false, true},
    {PermissionAction::IGNORED, RequestType::kNotifications,
     PermissionPromptDisposition::NOT_APPLICABLE, false, true},
    {PermissionAction::DISMISSED, RequestType::kNotifications,
     PermissionPromptDisposition::MODAL_DIALOG, true, false},
    {PermissionAction::GRANTED, RequestType::kNotifications,
     PermissionPromptDisposition::LOCATION_BAR_LEFT_QUIET_CHIP, false, false},
    {PermissionAction::DISMISSED, RequestType::kVrSession,
     PermissionPromptDisposition::ANCHORED_BUBBLE, true, false},
    {PermissionAction::IGNORED, RequestType::kCameraStream,
     PermissionPromptDisposition::LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE, false,
     false},
    {PermissionAction::DISMISSED, RequestType::kGeolocation,
     PermissionPromptDisposition::CUSTOM_MODAL_DIALOG, false, false},
    {PermissionAction::DENIED, RequestType::kNotifications,
     PermissionPromptDisposition::LOCATION_BAR_RIGHT_ANIMATED_ICON, true,
     false},
    {PermissionAction::GRANTED, RequestType::kNotifications,
     PermissionPromptDisposition::ANCHORED_BUBBLE, false, false},
    {PermissionAction::GRANTED_ONCE, RequestType::kNotifications,
     PermissionPromptDisposition::ANCHORED_BUBBLE, false, false},
};

const char kLegacyPrefs[] = R"({
      "notifications": [
        {"time": "%s", "action" : 1},
        {"time": "%s", "action" : 3}
      ]
      })";

class MockPermissionActionsHistoryObserver
    : public permissions::PermissionActionsHistory::Observer {
 public:
  void OnAutoGrantedHeuristically(
      const GURL& origin,
      ContentSettingsType content_setting) override {
    origin_ = origin;
    content_setting_ = content_setting;
    call_count_++;
  }

  int call_count() const { return call_count_; }
  const GURL& origin() const { return origin_; }
  ContentSettingsType content_setting() const { return content_setting_; }

 private:
  int call_count_ = 0;
  GURL origin_;
  ContentSettingsType content_setting_ = ContentSettingsType::DEFAULT;
};
}  // namespace

class PermissionActionHistoryTestBase : public testing::Test {
 public:
  PermissionActionHistoryTestBase() = default;
  ~PermissionActionHistoryTestBase() override = default;

  PermissionActionsHistory* GetPermissionActionsHistory() {
    return PermissionsClient::Get()->GetPermissionActionsHistory(
        &browser_context_);
  }
  void SetUp() override {
    testing::Test::SetUp();
    RecordSetUpActions();
  }

  PermissionActionHistoryTestBase(const PermissionActionHistoryTestBase&) =
      delete;
  PermissionActionHistoryTestBase& operator=(
      const PermissionActionHistoryTestBase&) = delete;

  std::vector<PermissionActionsHistory::Entry> GetHistory(
      std::optional<RequestType> type,
      PermissionActionsHistory::EntryFilter entry_filter =
          PermissionActionsHistory::EntryFilter::WANT_ALL_PROMPTS) {
    if (type.has_value()) {
      return GetPermissionActionsHistory()->GetHistory(
          base::Time(), type.value(), entry_filter);
    } else {
      return GetPermissionActionsHistory()->GetHistory(base::Time(),
                                                       entry_filter);
    }
  }

  void RecordSetUpActions() {
    const int64_t time =
        base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds();
    const std::string formatted_legacy_prefs =
        base::StringPrintf(kLegacyPrefs, base::NumberToString(time).c_str(),
                           base::NumberToString(time).c_str());
    std::optional<base::Value> legacy_pref_value = base::JSONReader::Read(
        formatted_legacy_prefs, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    GetPermissionActionsHistory()->GetPrefServiceForTesting()->Set(
        prefs::kPermissionActions, legacy_pref_value.value());
    // Record the actions needed to support test cases. This is the structure
    // 3-days ago: {notification, dismissed}
    // 2-days ago: {notification, granted}, {vr, dismissed}
    // 1-days ago: {geolocation, ignored}, {camera, dismissed}, {notification,
    // denied}
    // 0-days ago: {notification, granted}
    for (const auto& entry : kTestEntries) {
      // Legacy entries are added directly to the pref above and not through
      // Permission Actions History API
      if (entry.is_legacy_entry) {
        continue;
      }
      GetPermissionActionsHistory()->RecordAction(entry.action, entry.type,
                                                  entry.prompt_disposition);
      if (entry.advance_clock) {
        task_environment_.AdvanceClock(base::Days(1));
      }
    }
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::TestBrowserContext browser_context_;

 private:
  TestPermissionsClient permissions_client_;
};

class PermissionActionHistoryTest : public base::test::WithFeatureOverride,
                                    public PermissionActionHistoryTestBase {
 public:
  PermissionActionHistoryTest()
      : base::test::WithFeatureOverride(
            content_settings::features::kApproximateGeolocationPermission) {}
};

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PermissionActionHistoryTest);

TEST_P(PermissionActionHistoryTest, GetHistorySortedOrder) {
  auto all_entries = GetHistory(std::nullopt);

  EXPECT_EQ(10u, all_entries.size());

  size_t index = 0;
  for (const auto& entry : kTestEntries) {
    EXPECT_EQ(entry.action, all_entries[index++].action);
  }

  for (const auto& request_type :
       {RequestType::kVrSession, RequestType::kCameraStream,
        RequestType::kGeolocation, RequestType::kNotifications}) {
    auto permission_entries = GetHistory(request_type);

    index = 0;
    for (const auto& entry : kTestEntries) {
      if (entry.type != request_type) {
        continue;
      }

      EXPECT_EQ(entry.action, permission_entries[index++].action);
    }
  }

  auto entries_1_day = GetPermissionActionsHistory()->GetHistory(
      base::Time::Now() - base::Days(1),
      PermissionActionsHistory::EntryFilter::WANT_ALL_PROMPTS);

  EXPECT_TRUE(std::ranges::equal(
      entries_1_day, std::vector<PermissionActionsHistory::Entry>(
                         all_entries.begin() + 5, all_entries.end())));
}

TEST_P(PermissionActionHistoryTest, NotificationRecordAction) {
  size_t general_count = GetHistory(std::nullopt).size();
  size_t notification_count = GetHistory(RequestType::kNotifications).size();

  GetPermissionActionsHistory()->RecordAction(
      PermissionAction::GRANTED, RequestType::kNotifications,
      PermissionPromptDisposition::ANCHORED_BUBBLE);

  EXPECT_EQ(general_count + 1, GetHistory(std::nullopt).size());
  EXPECT_EQ(notification_count + 1,
            GetHistory(RequestType::kNotifications).size());

  GetPermissionActionsHistory()->RecordAction(
      PermissionAction::GRANTED, RequestType::kGeolocation,
      PermissionPromptDisposition::ANCHORED_BUBBLE);

  EXPECT_EQ(general_count + 2, GetHistory(std::nullopt).size());
  EXPECT_EQ(notification_count + 1,
            GetHistory(RequestType::kNotifications).size());
}

TEST_P(PermissionActionHistoryTest, ClearHistory) {
  struct {
    base::Time begin;
    base::Time end;
    size_t generic_count;
    size_t notifications_count;
  } kTests[] = {
      // Misc and baseline tests cases.
      {base::Time(), base::Time::Max(), 0, 0},
      {base::Time(), base::Time::Now(), 2, 2},
      {base::Time(), base::Time::Now() + base::Microseconds(1), 0, 0},

      // Test cases specifying only the upper bound.
      {base::Time(), base::Time::Now() - base::Days(1), 5, 3},
      {base::Time(), base::Time::Now() - base::Days(2), 7, 4},
      {base::Time(), base::Time::Now() - base::Days(3), 10, 7},

      // Test cases specifying only the lower bound.
      {base::Time::Now() - base::Days(3), base::Time::Max(), 0, 0},
      {base::Time::Now() - base::Days(2), base::Time::Max(), 3, 3},
      {base::Time::Now() - base::Days(1), base::Time::Max(), 5, 4},
      {base::Time::Now(), base::Time::Max(), 8, 5},

      // Test cases with both bounds.
      {base::Time::Now() - base::Days(3),
       base::Time::Now() + base::Microseconds(1), 0, 0},
      {base::Time::Now() - base::Days(3), base::Time::Now(), 2, 2},
      {base::Time::Now() - base::Days(3), base::Time::Now() - base::Days(1), 5,
       3},
      {base::Time::Now() - base::Days(3), base::Time::Now() - base::Days(2), 7,
       4},
      {base::Time::Now() - base::Days(3), base::Time::Now() - base::Days(3), 10,
       7},

      {base::Time::Now() - base::Days(2),
       base::Time::Now() + base::Microseconds(1), 3, 3},
      {base::Time::Now() - base::Days(2), base::Time::Now(), 5, 5},
      {base::Time::Now() - base::Days(2), base::Time::Now() - base::Days(1), 8,
       6},
      {base::Time::Now() - base::Days(2), base::Time::Now() - base::Days(2), 10,
       7},

      {base::Time::Now() - base::Days(1),
       base::Time::Now() + base::Microseconds(1), 5, 4},
      {base::Time::Now() - base::Days(1), base::Time::Now(), 7, 6},
      {base::Time::Now() - base::Days(1), base::Time::Now() - base::Days(1), 10,
       7},

      {base::Time::Now(), base::Time::Now() + base::Microseconds(1), 8, 5},
      {base::Time::Now(), base::Time::Now(), 10, 7},
  };

  // We need to account for much we have already advanced the time for each test
  // case and so we keep track of how much we need to offset the initial test
  // values.
  base::TimeDelta current_offset;

  for (auto& test : kTests) {
    test.begin += current_offset;
    test.end += current_offset;

    GetPermissionActionsHistory()->ClearHistory(test.begin, test.end);
    EXPECT_EQ(test.generic_count, GetHistory(std::nullopt).size());
    EXPECT_EQ(test.notifications_count,
              GetHistory(RequestType::kNotifications).size());

    // Clean up for next test and update offset.
    base::Time last_now = base::Time::Now();
    GetPermissionActionsHistory()->ClearHistory(base::Time(),
                                                base::Time::Max());
    RecordSetUpActions();
    current_offset += base::Time::Now() - last_now;
  }
}

TEST_P(PermissionActionHistoryTest, EntryFilterTest) {
  auto loud_entries =
      GetHistory(std::nullopt,
                 PermissionActionsHistory::EntryFilter::WANT_LOUD_PROMPTS_ONLY);
  EXPECT_EQ(6u, loud_entries.size());

  auto quiet_entries = GetHistory(
      std::nullopt, PermissionActionsHistory::PermissionActionsHistory::
                        EntryFilter::WANT_QUIET_PROMPTS_ONLY);
  EXPECT_EQ(2u, quiet_entries.size());

  auto all_entries = GetHistory(
      std::nullopt, PermissionActionsHistory::EntryFilter::WANT_ALL_PROMPTS);
  EXPECT_EQ(10u, all_entries.size());

  auto quiet_entries_in_last_two_days =
      GetPermissionActionsHistory()->GetHistory(
          base::Time::Now() - base::Days(2),
          PermissionActionsHistory::EntryFilter::WANT_QUIET_PROMPTS_ONLY);
  EXPECT_EQ(2u, quiet_entries_in_last_two_days.size());
}

TEST_P(PermissionActionHistoryTest, FillInActionCountsTest) {
  auto all_entries =
      GetHistory(RequestType::kNotifications,
                 PermissionActionsHistory::EntryFilter::WANT_ALL_PROMPTS);
  PredictionRequestFeatures::ActionCounts actions_counts;
  PermissionActionsHistory::FillInActionCounts(&actions_counts, all_entries);
  EXPECT_EQ(3u, actions_counts.grants);
  EXPECT_EQ(7u, actions_counts.total());

  // FillInActionCounts combines one time and permanent grants, but on a storage
  // level they are still stored separately.
  uint32_t permanent_grant_count = 0;
  uint32_t one_time_grant_count = 0;
  for (auto entry : all_entries) {
    if (entry.action == PermissionAction::GRANTED) {
      permanent_grant_count++;
    }
    if (entry.action == PermissionAction::GRANTED_ONCE) {
      one_time_grant_count++;
    }
  }
  EXPECT_EQ(2u, permanent_grant_count);
  EXPECT_EQ(1u, one_time_grant_count);
  EXPECT_EQ(7u, all_entries.size());
}

class PermissionActionHistoryHeuristicGrantTest
    : public PermissionActionHistoryTest {
 public:
  PermissionActionHistoryHeuristicGrantTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kGeolocationElement,
         permissions::features::kPermissionHeuristicAutoGrant},
        {});
  }
  PermissionActionHistoryHeuristicGrantTest(
      const PermissionActionHistoryHeuristicGrantTest&) = delete;
  ~PermissionActionHistoryHeuristicGrantTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    PermissionActionHistoryHeuristicGrantTest);

TEST_P(PermissionActionHistoryHeuristicGrantTest, HeuristicGrant) {
  GURL url("https://www.example.com");
  ContentSettingsType permission =
      content_settings::GeolocationContentSettingsType();
  auto* history = GetPermissionActionsHistory();

  MockPermissionActionsHistoryObserver observer;
  history->AddObserver(&observer);

  for (int i = 0; i < kHeuristicGrantThreshold; ++i) {
    EXPECT_FALSE(history->RecordTemporaryGrant(url, permission));
    EXPECT_EQ(0, observer.call_count());
  }

  // The next time should trigger auto-grant.
  EXPECT_TRUE(history->RecordTemporaryGrant(url, permission));
  EXPECT_EQ(1, observer.call_count());
  EXPECT_EQ(url, observer.origin());
  EXPECT_EQ(permission, observer.content_setting());

  // Subsequent calls should also return true.
  EXPECT_TRUE(history->RecordTemporaryGrant(url, permission));
  // The observer is notified again.
  EXPECT_EQ(2, observer.call_count());

  history->RemoveObserver(&observer);
}

TEST_P(PermissionActionHistoryHeuristicGrantTest, HeuristicGrantReset) {
  GURL url("https://www.example.com");
  ContentSettingsType permission =
      content_settings::GeolocationContentSettingsType();
  auto* history = GetPermissionActionsHistory();

  // Grant twice.
  EXPECT_FALSE(history->RecordTemporaryGrant(url, permission));
  EXPECT_FALSE(history->RecordTemporaryGrant(url, permission));

  // Reset.
  history->ResetHeuristicData(url, permission);

  for (int i = 0; i < kHeuristicGrantThreshold; ++i) {
    EXPECT_FALSE(history->RecordTemporaryGrant(url, permission));
  }

  // Next time after reset should trigger auto-grant.
  EXPECT_TRUE(history->RecordTemporaryGrant(url, permission));
}

TEST_P(PermissionActionHistoryHeuristicGrantTest,
       HeuristicGrantMultipleOriginsAndPermissions) {
  GURL url1("https://www.example.com");
  GURL url2("https://www.google.com");
  ContentSettingsType permission1 =
      content_settings::GeolocationContentSettingsType();
  auto* history = GetPermissionActionsHistory();

  for (int i = 0; i < kHeuristicGrantThreshold - 1; ++i) {
    history->RecordTemporaryGrant(url1, permission1);
    history->RecordTemporaryGrant(url2, permission1);
  }

  // Grant url1/permission1 one more time. Should not auto-grant.
  EXPECT_FALSE(history->RecordTemporaryGrant(url1, permission1));

  // Grant url1/permission1 another time. Next check will auto-grant.
  EXPECT_TRUE(history->RecordTemporaryGrant(url1, permission1));

  // The other permissions should not be auto-granted yet.
  // The next call will increment to counter and not auto-grant.
  EXPECT_FALSE(history->RecordTemporaryGrant(url2, permission1));

  // The next call for these will auto-grant.
  EXPECT_TRUE(history->RecordTemporaryGrant(url2, permission1));
}

TEST_P(PermissionActionHistoryHeuristicGrantTest,
       HeuristicGrantGeolocationOnly) {
  GURL url("https://www.example.com");
  auto* history = GetPermissionActionsHistory();

  // GEOLOCATION should work.
  EXPECT_FALSE(history->RecordTemporaryGrant(
      url, content_settings::GeolocationContentSettingsType()));

  // NOTIFICATIONS should crash.
  EXPECT_DEATH_IF_SUPPORTED(
      history->RecordTemporaryGrant(url, ContentSettingsType::NOTIFICATIONS),
      "");
  EXPECT_DEATH_IF_SUPPORTED(history->CheckHeuristicallyAutoGranted(
                                url, ContentSettingsType::NOTIFICATIONS),
                            "");
}

TEST_P(PermissionActionHistoryHeuristicGrantTest,
       CheckHeuristicallyAutoGranted) {
  GURL url("https://www.example.com");
  ContentSettingsType permission =
      content_settings::GeolocationContentSettingsType();
  auto* history = GetPermissionActionsHistory();

  EXPECT_FALSE(history->CheckHeuristicallyAutoGranted(url, permission,
                                                      /*needs_update*/ false));

  for (int i = 0; i < kHeuristicGrantThreshold; ++i) {
    history->RecordTemporaryGrant(url, permission);
  }

  // Trigger auto-grant.
  EXPECT_TRUE(history->RecordTemporaryGrant(url, permission));
  EXPECT_TRUE(history->CheckHeuristicallyAutoGranted(url, permission,
                                                     /*needs_update*/ false));

  // Advance clock past expiration date.
  task_environment_.AdvanceClock(base::Days(29));
  EXPECT_FALSE(history->CheckHeuristicallyAutoGranted(url, permission,
                                                      /*needs_update*/ false));
}

TEST_P(PermissionActionHistoryHeuristicGrantTest,
       CheckHeuristicallyAutoGrantedNeedsUpdate) {
  GURL url("https://www.example.com");
  ContentSettingsType permission =
      content_settings::GeolocationContentSettingsType();
  auto* history = GetPermissionActionsHistory();

  // Ensure it's not auto-granted initially.
  EXPECT_FALSE(history->CheckHeuristicallyAutoGranted(url, permission,
                                                      /*needs_update*/ false));

  // Trigger auto-grant.
  for (int i = 0; i < kHeuristicGrantThreshold; ++i) {
    history->RecordTemporaryGrant(url, permission);
  }
  EXPECT_TRUE(history->RecordTemporaryGrant(url, permission));

  // Check with needs_update = true. Timestamp should change.
  task_environment_.AdvanceClock(base::Days(2));
  EXPECT_TRUE(history->CheckHeuristicallyAutoGranted(url, permission));
  task_environment_.AdvanceClock(base::Days(6));
  EXPECT_TRUE(history->CheckHeuristicallyAutoGranted(url, permission,
                                                     /*needs_update*/ false));
  task_environment_.AdvanceClock(base::Days(20));
  EXPECT_TRUE(history->CheckHeuristicallyAutoGranted(url, permission,
                                                     /*needs_update*/ false));
  task_environment_.AdvanceClock(base::Days(3));
  EXPECT_FALSE(history->CheckHeuristicallyAutoGranted(url, permission,
                                                      /*needs_update*/ false));
}

TEST_P(PermissionActionHistoryHeuristicGrantTest,
       HeuristicGrantExpirationDecaysCount) {
  GURL url("https://www.example.com");
  ContentSettingsType permission =
      content_settings::GeolocationContentSettingsType();
  auto* history = GetPermissionActionsHistory();

  // Grant up to the threshold to enable auto-granting.
  for (int i = 0; i < kHeuristicGrantThreshold; ++i) {
    history->RecordTemporaryGrant(url, permission);
  }

  // The next grant will trigger auto-grant.
  EXPECT_TRUE(history->RecordTemporaryGrant(url, permission));
  EXPECT_EQ(kHeuristicGrantThreshold + 1,
            history->GetTemporaryGrantCountForTesting(url, permission));
  EXPECT_TRUE(history->CheckHeuristicallyAutoGranted(url, permission,
                                                     /*needs_update*/ false));

  // Advance clock past expiration date.
  task_environment_.AdvanceClock(base::Days(29));

  // The grant should have expired.
  EXPECT_FALSE(history->CheckHeuristicallyAutoGranted(url, permission,
                                                      /*needs_update*/ false));

  // The count should have decayed to 2.
  EXPECT_EQ(2, history->GetTemporaryGrantCountForTesting(url, permission));

  // Advance clock past expiration date again.
  task_environment_.AdvanceClock(base::Days(29));

  // The grant should still be expired.
  EXPECT_FALSE(history->CheckHeuristicallyAutoGranted(url, permission,
                                                      /*needs_update*/ false));
  // The count should have decayed to 0.
  EXPECT_EQ(0, history->GetTemporaryGrantCountForTesting(url, permission));

  // The next grant should not auto-grant, but increment the count to 1.
  EXPECT_FALSE(history->RecordTemporaryGrant(url, permission));
  EXPECT_EQ(1, history->GetTemporaryGrantCountForTesting(url, permission));

  // Grant twice more to reach the threshold.
  EXPECT_FALSE(history->RecordTemporaryGrant(url, permission));
  EXPECT_FALSE(history->RecordTemporaryGrant(url, permission));
  EXPECT_EQ(3, history->GetTemporaryGrantCountForTesting(url, permission));

  // The next grant should now trigger auto-grant.
  EXPECT_TRUE(history->RecordTemporaryGrant(url, permission));
  EXPECT_EQ(4, history->GetTemporaryGrantCountForTesting(url, permission));
}

TEST_P(PermissionActionHistoryHeuristicGrantTest,
       HeuristicGrantResetWithFilter) {
  GURL url1("https://www.example.com");
  GURL url2("https://www.google.com");
  ContentSettingsType permission =
      content_settings::GeolocationContentSettingsType();
  auto* history = GetPermissionActionsHistory();

  // Grant url1 and url2 twice.
  EXPECT_FALSE(history->RecordTemporaryGrant(url1, permission));
  EXPECT_FALSE(history->RecordTemporaryGrant(url1, permission));

  EXPECT_FALSE(history->RecordTemporaryGrant(url2, permission));
  EXPECT_FALSE(history->RecordTemporaryGrant(url2, permission));

  // Reset for urls matching "example.com".
  history->ResetHeuristicData(base::BindRepeating(
      [](const GURL& url) { return url.GetHost() == "www.example.com"; }));

  // The counter for url1 should be reset. It should take
  // `kHeuristicGrantThreshold` more grants to trigger auto-grant.
  for (int i = 0; i < kHeuristicGrantThreshold; ++i) {
    EXPECT_FALSE(history->RecordTemporaryGrant(url1, permission));
  }
  EXPECT_TRUE(history->RecordTemporaryGrant(url1, permission));

  // The counter for url2 should not be reset. It was granted twice, so it
  // needs one more grant to reach the threshold.
  EXPECT_FALSE(history->RecordTemporaryGrant(url2, permission));
  // The next one should auto-grant.
  EXPECT_TRUE(history->RecordTemporaryGrant(url2, permission));
}

TEST_P(PermissionActionHistoryTest, RecordOneTimeGrant) {
  GURL url1("https://www.example.com");
  GURL url2("https://www.google.com");
  auto* history = GetPermissionActionsHistory();
  base::HistogramTester histogram_tester;

  // Geolocation
  history->RecordOneTimeGrant(
      url1, content_settings::GeolocationContentSettingsType());
  histogram_tester.ExpectBucketCount(
      "Permissions.OneTimePermission.Geolocation.OneTimeGrant", 1, 1);
  history->RecordOneTimeGrant(
      url1, content_settings::GeolocationContentSettingsType());
  histogram_tester.ExpectBucketCount(
      "Permissions.OneTimePermission.Geolocation.OneTimeGrant", 2, 1);
  history->RecordOneTimeGrant(
      url2, content_settings::GeolocationContentSettingsType());
  histogram_tester.ExpectBucketCount(
      "Permissions.OneTimePermission.Geolocation.OneTimeGrant", 1, 2);

  // Mic
  history->RecordOneTimeGrant(url1, ContentSettingsType::MEDIASTREAM_MIC);
  histogram_tester.ExpectBucketCount(
      "Permissions.OneTimePermission.AudioCapture.OneTimeGrant", 1, 1);
  history->RecordOneTimeGrant(url1, ContentSettingsType::MEDIASTREAM_MIC);
  histogram_tester.ExpectBucketCount(
      "Permissions.OneTimePermission.AudioCapture.OneTimeGrant", 2, 1);

  // Camera
  history->RecordOneTimeGrant(url1, ContentSettingsType::MEDIASTREAM_CAMERA);
  histogram_tester.ExpectBucketCount(
      "Permissions.OneTimePermission.VideoCapture.OneTimeGrant", 1, 1);

  // Unsupported type - should be ignored
  history->RecordOneTimeGrant(url1, ContentSettingsType::NOTIFICATIONS);
  histogram_tester.ExpectTotalCount(
      "Permissions.OneTimePermission.Notifications.OneTimeGrant", 0);

  // Check total counts
  histogram_tester.ExpectTotalCount(
      "Permissions.OneTimePermission.Geolocation.OneTimeGrant", 3);
  histogram_tester.ExpectTotalCount(
      "Permissions.OneTimePermission.AudioCapture.OneTimeGrant", 2);

  histogram_tester.ExpectTotalCount(
      "Permissions.OneTimePermission.VideoCapture.OneTimeGrant", 1);
}

struct PermissionActionsHistoryTestParams {
  ContentSettingsType type;
  std::string permission_name;
  PermissionAction action;
  std::string suffix;
  bool approximate_geolocation_enabled = false;
};

class PermissionActionHistoryParameterizedTest
    : public PermissionActionHistoryTestBase,
      public testing::WithParamInterface<PermissionActionsHistoryTestParams> {
 public:
  PermissionActionHistoryParameterizedTest() {
    scoped_feature_list_.InitWithFeatureState(
        content_settings::features::kApproximateGeolocationPermission,
        GetParam().approximate_geolocation_enabled);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(PermissionActionHistoryParameterizedTest, RecordOTPCountForAction) {
  auto* history = GetPermissionActionsHistory();
  base::HistogramTester histogram_tester;

  std::string histogram_name = "Permissions.OneTimePermission." +
                               GetParam().permission_name + "." +
                               GetParam().suffix;
  history->RecordOTPCountForAction(GetParam().type, GetParam().action, 0);
  histogram_tester.ExpectBucketCount(histogram_name, 0, 1);

  history->RecordOTPCountForAction(GetParam().type, GetParam().action, 1);
  histogram_tester.ExpectBucketCount(histogram_name, 1, 1);

  history->RecordOTPCountForAction(GetParam().type, GetParam().action, 5);
  histogram_tester.ExpectBucketCount(histogram_name, 5, 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PermissionActionHistoryParameterizedTest,
    testing::Values(
        // Geolocation
        PermissionActionsHistoryTestParams{
            ContentSettingsType::GEOLOCATION, "Geolocation",
            PermissionAction::GRANTED, "GrantOTPCount"},
        PermissionActionsHistoryTestParams{
            ContentSettingsType::GEOLOCATION, "Geolocation",
            PermissionAction::DENIED, "DenyOTPCount"},
        PermissionActionsHistoryTestParams{
            ContentSettingsType::GEOLOCATION, "Geolocation",
            PermissionAction::DISMISSED, "DismissOTPCount"},
        PermissionActionsHistoryTestParams{
            ContentSettingsType::GEOLOCATION, "Geolocation",
            PermissionAction::IGNORED, "IgnoreOTPCount"},
        PermissionActionsHistoryTestParams{
            ContentSettingsType::GEOLOCATION_WITH_OPTIONS, "Geolocation",
            PermissionAction::GRANTED, "GrantOTPCount", true},
        PermissionActionsHistoryTestParams{
            ContentSettingsType::GEOLOCATION_WITH_OPTIONS, "Geolocation",
            PermissionAction::DENIED, "DenyOTPCount", true},
        PermissionActionsHistoryTestParams{
            ContentSettingsType::GEOLOCATION_WITH_OPTIONS, "Geolocation",
            PermissionAction::DISMISSED, "DismissOTPCount", true},
        PermissionActionsHistoryTestParams{
            ContentSettingsType::GEOLOCATION_WITH_OPTIONS, "Geolocation",
            PermissionAction::IGNORED, "IgnoreOTPCount", true},
        // Mic
        PermissionActionsHistoryTestParams{
            ContentSettingsType::MEDIASTREAM_MIC, "AudioCapture",
            PermissionAction::GRANTED, "GrantOTPCount"},
        PermissionActionsHistoryTestParams{
            ContentSettingsType::MEDIASTREAM_MIC, "AudioCapture",
            PermissionAction::DENIED, "DenyOTPCount"},
        PermissionActionsHistoryTestParams{
            ContentSettingsType::MEDIASTREAM_MIC, "AudioCapture",
            PermissionAction::DISMISSED, "DismissOTPCount"},
        PermissionActionsHistoryTestParams{
            ContentSettingsType::MEDIASTREAM_MIC, "AudioCapture",
            PermissionAction::IGNORED, "IgnoreOTPCount"},
        // Camera
        PermissionActionsHistoryTestParams{
            ContentSettingsType::MEDIASTREAM_CAMERA, "VideoCapture",
            PermissionAction::GRANTED, "GrantOTPCount"},
        PermissionActionsHistoryTestParams{
            ContentSettingsType::MEDIASTREAM_CAMERA, "VideoCapture",
            PermissionAction::DENIED, "DenyOTPCount"},
        PermissionActionsHistoryTestParams{
            ContentSettingsType::MEDIASTREAM_CAMERA, "VideoCapture",
            PermissionAction::DISMISSED, "DismissOTPCount"},
        PermissionActionsHistoryTestParams{
            ContentSettingsType::MEDIASTREAM_CAMERA, "VideoCapture",
            PermissionAction::IGNORED, "IgnoreOTPCount"}),
    [](const testing::TestParamInfo<PermissionActionsHistoryTestParams>& info) {
      return info.param.permission_name + "_" + info.param.suffix +
             (info.param.approximate_geolocation_enabled
                  ? "_withApproximateLocation"
                  : "");
    });

TEST_P(PermissionActionHistoryTest, RecordOTPCountForAction_UnsupportedType) {
  auto* history = GetPermissionActionsHistory();
  base::HistogramTester histogram_tester;

  // Unsupported type - should be ignored
  history->RecordOTPCountForAction(ContentSettingsType::NOTIFICATIONS,
                                   PermissionAction::DENIED, 2);
  history->RecordOTPCountForAction(ContentSettingsType::NOTIFICATIONS,
                                   PermissionAction::DISMISSED, 2);
  history->RecordOTPCountForAction(ContentSettingsType::NOTIFICATIONS,
                                   PermissionAction::IGNORED, 2);
  history->RecordOTPCountForAction(ContentSettingsType::NOTIFICATIONS,
                                   PermissionAction::GRANTED, 2);

  histogram_tester.ExpectTotalCount(
      "Permissions.OneTimePermission.Notifications.DenyOTPCount", 0);
  histogram_tester.ExpectTotalCount(
      "Permissions.OneTimePermission.Notifications.DismissOTPCount", 0);
  histogram_tester.ExpectTotalCount(
      "Permissions.OneTimePermission.Notifications.IgnoreOTPCount", 0);
  histogram_tester.ExpectTotalCount(
      "Permissions.OneTimePermission.Notifications.GrantOTPCount", 0);
}

TEST_P(PermissionActionHistoryParameterizedTest, GetOneTimeGrantCount) {
  GURL url1("https://www.example.com");
  GURL url2("https://www.google.com");
  auto* history = GetPermissionActionsHistory();

  EXPECT_EQ(0, history->GetOneTimeGrantCount(url1, GetParam().type));

  // Record some GRANTED_ONCE actions
  history->RecordOneTimeGrant(url1, GetParam().type);
  EXPECT_EQ(1, history->GetOneTimeGrantCount(url1, GetParam().type));
  EXPECT_EQ(0, history->GetOneTimeGrantCount(url2, GetParam().type));

  history->RecordOneTimeGrant(url1, GetParam().type);
  EXPECT_EQ(2, history->GetOneTimeGrantCount(url1, GetParam().type));

  history->RecordOneTimeGrant(url2, GetParam().type);
  EXPECT_EQ(1, history->GetOneTimeGrantCount(url2, GetParam().type));
  EXPECT_EQ(2, history->GetOneTimeGrantCount(
                   url1, GetParam().type));  // url1 unchanged

  // Non-one-time grant actions should not affect the count
  RequestType request_type =
      GetParam().type == content_settings::GeolocationContentSettingsType()
          ? RequestType::kGeolocation
          : (GetParam().type == ContentSettingsType::MEDIASTREAM_MIC
                 ? RequestType::kMicStream
                 : RequestType::kCameraStream);
  history->RecordAction(GetParam().action, request_type,
                        PermissionPromptDisposition::ANCHORED_BUBBLE);
  EXPECT_EQ(2, history->GetOneTimeGrantCount(url1, GetParam().type));
}

TEST_P(PermissionActionHistoryTest, GetOneTimeGrantCount_UnsupportedType) {
  GURL url1("https://www.example.com");
  auto* history = GetPermissionActionsHistory();

  history->RecordOneTimeGrant(url1, ContentSettingsType::NOTIFICATIONS);
  EXPECT_EQ(0, history->GetOneTimeGrantCount(
                   url1, ContentSettingsType::NOTIFICATIONS));
}

}  // namespace permissions
