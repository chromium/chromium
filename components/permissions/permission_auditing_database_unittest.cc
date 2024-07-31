// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/permissions/permission_auditing_database.h"

#include <memory>

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "sql/test/scoped_error_expecter.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace permissions {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

namespace {

base::Time TimeFromTimestamp(const int64_t& time) {
  return base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(time));
}

constexpr ContentSettingsType kTestTypes[] = {
    ContentSettingsType::GEOLOCATION, ContentSettingsType::NOTIFICATIONS};

constexpr char kTestUrl1[] = "http://www.example1.com";
constexpr char kTestUrl2[] = "http://www.example2.com";

std::string GetUniqueUrl(int id) {
  return base::StringPrintf("http://www.example%d.com", id);
}

url::Origin GetOrigin(const char* url) {
  return url::Origin::Create(GURL(url));
}

PermissionUsageSession SessionLike(ContentSettingsType type,
                                   const char* url,
                                   const PermissionUsageSession& session) {
  return {.origin = GetOrigin(url),
          .type = type,
          .usage_start = session.usage_start,
          .usage_end = session.usage_end,
          .had_user_activation = session.had_user_activation,
          .was_foreground = session.was_foreground,
          .had_focus = session.had_focus};
}

}  // namespace

class PermissionAuditingDatabaseTest : public testing::Test {
 public:
  PermissionAuditingDatabaseTest()
      : test_sessions_(GeneratePermissionSessions()) {}

  PermissionAuditingDatabaseTest(const PermissionAuditingDatabaseTest&) =
      delete;
  PermissionAuditingDatabaseTest& operator=(
      const PermissionAuditingDatabaseTest&) = delete;

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());
    path_ = temp_directory_.GetPath().Append(
        FILE_PATH_LITERAL("test_permission_auditing_database"));
    ASSERT_TRUE(db_.Init(path_));
  }

  std::vector<PermissionUsageSession> GetPermissionUsageHistory(
      ContentSettingsType type,
      const char* url,
      base::Time starting_from = base::Time()) {
    return db_.GetPermissionUsageHistory(type, GetOrigin(url), starting_from);
  }

  std::optional<base::Time> GetLastUsageTime(ContentSettingsType type,
                                             const char* url) {
    return db_.GetLastPermissionUsageTime(type, GetOrigin(url));
  }

  bool UpdateEndTime(ContentSettingsType type,
                     const char* url,
                     base::Time ordering_time,
                     base::Time new_end_time) {
    return db_.UpdateEndTime(type, GetOrigin(url), ordering_time, new_end_time);
  }

  void StoreSessionForEachTime() {
    for (const auto& time : test_times_) {
      ASSERT_TRUE(db().StorePermissionUsage({.origin = GetOrigin(kTestUrl1),
                                             .type = kTestTypes[0],
                                             .usage_start = time,
                                             .usage_end = time,
                                             .had_user_activation = false,
                                             .was_foreground = false,
                                             .had_focus = false}));
    }
  }

  PermissionAuditingDatabase& db() { return db_; }

  const base::Time test_times_[3] = {TimeFromTimestamp(12864787200000000),
                                     TimeFromTimestamp(12864787200000001),
                                     TimeFromTimestamp(12864787200000002)};

  const std::vector<PermissionUsageSession> test_sessions_;

 private:
  std::vector<PermissionUsageSession> GeneratePermissionSessions() {
    std::vector<PermissionUsageSession> sessions;
    for (size_t i = 0; i < std::size(test_times_); ++i) {
      for (size_t j = i + 1; j <= std::size(test_times_); ++j) {
        for (bool had_user_activation : {false, true}) {
          for (bool was_foreground : {false, true}) {
            for (bool had_focus : {false, true}) {
              base::Time start = test_times_[i];
              base::Time end = (j == std::size(test_times_)) ? test_times_[i]
                                                             : test_times_[j];
              sessions.push_back({.usage_start = start,
                                  .usage_end = end,
                                  .had_user_activation = had_user_activation,
                                  .was_foreground = was_foreground,
                                  .had_focus = had_focus});
            }
          }
        }
      }
    }
    return sessions;
  }

  PermissionAuditingDatabase db_;

  base::ScopedTempDir temp_directory_;
  base::FilePath path_;
};

TEST_F(PermissionAuditingDatabaseTest, IsUsageHistorySizeCorrect) {
  auto session = SessionLike(kTestTypes[0], kTestUrl1, test_sessions_[0]);
  EXPECT_THAT(GetPermissionUsageHistory(kTestTypes[0], kTestUrl1), IsEmpty());
  size_t current_size = 0;
  for (const auto& time : test_times_) {
    session.usage_start = time;
    session.usage_end = time;
    ASSERT_TRUE(db().StorePermissionUsage(
        SessionLike(kTestTypes[0], kTestUrl1, session)));
    ASSERT_EQ(GetPermissionUsageHistory(kTestTypes[0], kTestUrl1).size(),
              ++current_size);
  }
}

TEST_F(PermissionAuditingDatabaseTest,
       IsUsageHistoryDifferentForDifferentPermissionsAndOrigins) {
  const auto& session1 =
      SessionLike(kTestTypes[0], kTestUrl1, test_sessions_[0]);
  const auto& session2 =
      SessionLike(kTestTypes[1], kTestUrl2, test_sessions_[1]);
  ASSERT_TRUE(db().StorePermissionUsage(session1));
  ASSERT_TRUE(db().StorePermissionUsage(session2));

  EXPECT_THAT(GetPermissionUsageHistory(kTestTypes[0], kTestUrl1),
              ElementsAre(session1));
  EXPECT_THAT(GetPermissionUsageHistory(kTestTypes[1], kTestUrl2),
              ElementsAre(session2));
  EXPECT_THAT(GetPermissionUsageHistory(kTestTypes[0], kTestUrl2), IsEmpty());
  EXPECT_THAT(GetPermissionUsageHistory(kTestTypes[1], kTestUrl1), IsEmpty());
}

TEST_F(PermissionAuditingDatabaseTest, AreFieldsStoredCorrectlyInUsageHistory) {
  int counter = 0;
  for (const auto& session : test_sessions_) {
    const std::string url = GetUniqueUrl(++counter);
    auto updated_session = SessionLike(kTestTypes[0], url.c_str(), session);
    ASSERT_TRUE(db().StorePermissionUsage(updated_session));
    EXPECT_THAT(GetPermissionUsageHistory(kTestTypes[0], url.c_str()),
                ElementsAre(updated_session));
  }
}

TEST_F(PermissionAuditingDatabaseTest, UsageHistoryContainsOnlyLastSessions) {
  for (const auto time : test_times_) {
    ASSERT_TRUE(
        db().StorePermissionUsage({.origin = GetOrigin(kTestUrl1),
                                   .type = kTestTypes[0],
                                   .usage_start = time,
                                   .usage_end = time + base::Microseconds(1),
                                   .had_user_activation = false,
                                   .was_foreground = false,
                                   .had_focus = false}));
  }
  EXPECT_EQ(GetPermissionUsageHistory(kTestTypes[0], kTestUrl1).size(),
            std::size(test_times_));
  for (size_t i = 0; i < std::size(test_times_); ++i) {
    EXPECT_EQ(GetPermissionUsageHistory(kTestTypes[0], kTestUrl1,
                                        test_times_[i] + base::Microseconds(2))
                  .size(),
              std::size(test_times_) - i - 1);
  }
}

TEST_F(PermissionAuditingDatabaseTest, GetLastPermissionUsageTime) {
  EXPECT_FALSE(GetLastUsageTime(kTestTypes[0], kTestUrl1));
  for (const auto& time : test_times_) {
    ASSERT_TRUE(db().StorePermissionUsage({.origin = GetOrigin(kTestUrl1),
                                           .type = kTestTypes[0],
                                           .usage_start = time,
                                           .usage_end = time,
                                           .had_user_activation = false,
                                           .was_foreground = false,
                                           .had_focus = false}));
    EXPECT_EQ(GetLastUsageTime(kTestTypes[0], kTestUrl1), time);
  }
}

TEST_F(PermissionAuditingDatabaseTest, UpdateEndTime) {
  int counter = 0;
  for (const auto& session : test_sessions_) {
    const std::string url = GetUniqueUrl(++counter);
    ASSERT_TRUE(db().StorePermissionUsage(
        SessionLike(kTestTypes[0], url.c_str(), session)));
    const auto& end_time = session.usage_end;
    auto tomorrow = end_time + base::Days(1);
    ASSERT_TRUE(GetLastUsageTime(kTestTypes[0], url.c_str()) == end_time);
    ASSERT_TRUE(UpdateEndTime(kTestTypes[0], url.c_str(), session.usage_start,
                              tomorrow));
    EXPECT_EQ(GetLastUsageTime(kTestTypes[0], url.c_str()), tomorrow);
    auto history = GetPermissionUsageHistory(kTestTypes[0], url.c_str());
    ASSERT_EQ(history.size(), 1u);
    EXPECT_EQ(history[0].usage_end, tomorrow);
  }
}

TEST_F(PermissionAuditingDatabaseTest, DeleteSessionsBetween) {
  size_t current_size = std::size(test_times_);
  StoreSessionForEachTime();
  for (const auto& time : test_times_) {
    ASSERT_TRUE(db().DeleteSessionsBetween(time, time));
    ASSERT_EQ(GetPermissionUsageHistory(kTestTypes[0], kTestUrl1).size(),
              --current_size);
  }
}

TEST_F(PermissionAuditingDatabaseTest,
       DeleteSessionsBetweenWithUnspecifiedEndTime) {
  StoreSessionForEachTime();
  ASSERT_TRUE(db().DeleteSessionsBetween(test_times_[1], base::Time()));
  ASSERT_EQ(GetPermissionUsageHistory(kTestTypes[0], kTestUrl1).size(), 1u);
  ASSERT_TRUE(db().DeleteSessionsBetween(test_times_[0], base::Time()));
  EXPECT_THAT(GetPermissionUsageHistory(kTestTypes[0], kTestUrl1), IsEmpty());
}

TEST_F(PermissionAuditingDatabaseTest,
       DeleteSessionsBetweenWithUnspecifiedStartTime) {
  StoreSessionForEachTime();
  ASSERT_TRUE(db().DeleteSessionsBetween(
      base::Time(), test_times_[std::size(test_times_) - 2]));
  ASSERT_EQ(GetPermissionUsageHistory(kTestTypes[0], kTestUrl1).size(), 1u);
  ASSERT_TRUE(db().DeleteSessionsBetween(
      base::Time(), test_times_[std::size(test_times_) - 1]));
  EXPECT_THAT(GetPermissionUsageHistory(kTestTypes[0], kTestUrl1), IsEmpty());
}

TEST_F(PermissionAuditingDatabaseTest,
       DeleteSessionsBetweenWithUnspecifiedStartAndEndTime) {
  StoreSessionForEachTime();
  ASSERT_EQ(GetPermissionUsageHistory(kTestTypes[0], kTestUrl1).size(),
            std::size(test_times_));
  ASSERT_TRUE(db().DeleteSessionsBetween(base::Time(), base::Time()));
  EXPECT_THAT(GetPermissionUsageHistory(kTestTypes[0], kTestUrl1), IsEmpty());
}

TEST_F(PermissionAuditingDatabaseTest,
       StorePermissionUsageChecksTimerangeConstraint) {
  auto session = SessionLike(kTestTypes[0], kTestUrl1, test_sessions_[0]);
  session.usage_start = session.usage_end;
  EXPECT_TRUE(db().StorePermissionUsage(session));
}

TEST_F(PermissionAuditingDatabaseTest,
       StorePermissionUsageDoesntAccpetExistingRecord) {
  auto session = SessionLike(kTestTypes[0], kTestUrl1, test_sessions_[0]);
  EXPECT_TRUE(db().StorePermissionUsage(session));
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CONSTRAINT);
    EXPECT_FALSE(db().StorePermissionUsage(session));
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }
}

TEST_F(PermissionAuditingDatabaseTest, UpdateEndTimeChecksTimerangeConstraint) {
  auto session = SessionLike(kTestTypes[0], kTestUrl1, test_sessions_[0]);
  ASSERT_TRUE(db().StorePermissionUsage(session));
  EXPECT_TRUE(UpdateEndTime(kTestTypes[0], kTestUrl1, session.usage_start,
                            session.usage_start));
}

}  // namespace permissions
