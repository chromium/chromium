// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_actions_history.h"

#include <optional>
#include <vector>

#include "base/containers/adapters.h"
#include "base/json/json_reader.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
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

namespace permissions {
namespace {

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
     PermissionPromptDisposition::LOCATION_BAR_LEFT_CHIP, false, false},
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
}  // namespace

class PermissionActionHistoryTest : public testing::Test {
 public:
  PermissionActionHistoryTest() = default;
  ~PermissionActionHistoryTest() override = default;

  PermissionActionsHistory* GetPermissionActionsHistory() {
    return PermissionsClient::Get()->GetPermissionActionsHistory(
        &browser_context_);
  }
  void SetUp() override {
    testing::Test::SetUp();
    RecordSetUpActions();
  }

  PermissionActionHistoryTest(const PermissionActionHistoryTest&) = delete;
  PermissionActionHistoryTest& operator=(const PermissionActionHistoryTest&) =
      delete;

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
    std::optional<base::Value> legacy_pref_value =
        base::JSONReader::Read(formatted_legacy_prefs);
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
      if (entry.is_legacy_entry)
        continue;
      GetPermissionActionsHistory()->RecordAction(entry.action, entry.type,
                                                  entry.prompt_disposition);
      if (entry.advance_clock)
        task_environment_.AdvanceClock(base::Days(1));
    }
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::TestBrowserContext browser_context_;
  TestPermissionsClient permissions_client_;
};

TEST_F(PermissionActionHistoryTest, GetHistorySortedOrder) {
  auto all_entries = GetHistory(std::nullopt);

  EXPECT_EQ(10u, all_entries.size());

  size_t index = 0;
  for (const auto& entry : kTestEntries)
    EXPECT_EQ(entry.action, all_entries[index++].action);

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

  EXPECT_TRUE(base::ranges::equal(
      entries_1_day, std::vector<PermissionActionsHistory::Entry>(
                         all_entries.begin() + 5, all_entries.end())));
}

TEST_F(PermissionActionHistoryTest, NotificationRecordAction) {
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

TEST_F(PermissionActionHistoryTest, ClearHistory) {
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

TEST_F(PermissionActionHistoryTest, EntryFilterTest) {
  auto loud_entries =
      GetHistory(std::nullopt,
                 PermissionActionsHistory::EntryFilter::WANT_LOUD_PROMPTS_ONLY);
  EXPECT_EQ(5u, loud_entries.size());

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

TEST_F(PermissionActionHistoryTest, FillInActionCountsTest) {
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

}  // namespace permissions
