// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/permissions/permission_auditing_service.h"

#include <memory>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/permissions/permission_usage_session.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace permissions {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

namespace {

constexpr base::Time kTestTimes[] = {base::Time() + base::Days(1),
                                     base::Time() + base::Days(2),
                                     base::Time() + base::Days(3)};

constexpr ContentSettingsType kTestTypes[] = {
    ContentSettingsType::MEDIASTREAM_MIC,
    ContentSettingsType::MEDIASTREAM_CAMERA};

constexpr const char* kTestOrigins[] = {
    "https://foo1.com", "https://foo2.com", "https://foo3.com",
    "https://foo4.com", "https://foo5.com",
};

PermissionUsageSession BuildUsageSession(base::Time usage_start_time,
                                         base::Time usage_end_time) {
  return {.origin = url::Origin::Create(GURL(kTestOrigins[0])),
          .type = kTestTypes[0],
          .usage_start = usage_start_time,
          .usage_end = usage_end_time,
          .had_user_activation = false,
          .was_foreground = false,
          .had_focus = false};
}

PermissionUsageSession BuildUsageSession(ContentSettingsType type,
                                         const url::Origin& origin) {
  return {.origin = origin,
          .type = type,
          .usage_start = kTestTimes[0],
          .usage_end = kTestTimes[1],
          .had_user_activation = false,
          .was_foreground = false,
          .had_focus = false};
}

}  // namespace

class PermissionAuditingServiceTest : public testing::Test {
 public:
  PermissionAuditingServiceTest() = default;

 protected:
  PermissionAuditingService& service() { return *service_.get(); }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  void SetUp() override {
    backend_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
    service_ =
        std::make_unique<PermissionAuditingService>(backend_task_runner_);
    ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());
    base::FilePath database_path = temp_directory_.GetPath().Append(
        FILE_PATH_LITERAL("test_permission_auditing_database"));
    service_->Init(database_path);
    service_->StartPeriodicCullingOfExpiredSessions();
  }

  void TearDown() override {
    // Ensure the database is destroyed on the |backend_task_runner_|.
    service_.reset();
    task_environment_.RunUntilIdle();
  }

  std::vector<PermissionUsageSession> GetPermissionUsageHistory(
      ContentSettingsType type,
      const url::Origin& origin,
      base::Time start_time) {
    base::RunLoop run_loop;
    std::vector<PermissionUsageSession> history;
    service().GetPermissionUsageHistory(
        type, origin, start_time,
        base::BindLambdaForTesting(
            [&](std::vector<PermissionUsageSession> sessions) {
              history = std::move(sessions);
              run_loop.QuitWhenIdle();
            }));
    run_loop.Run();
    return history;
  }

  base::Time GetLastPermissionUsageTime(ContentSettingsType type,
                                        const url::Origin& origin) {
    base::RunLoop run_loop;
    base::Time last_usage_time;
    service().GetLastPermissionUsageTime(
        type, origin,
        base::BindLambdaForTesting([&](std::optional<base::Time> time) {
          last_usage_time = time.value_or(base::Time());
          run_loop.QuitWhenIdle();
        }));
    run_loop.Run();
    return last_usage_time;
  }

 private:
  base::ScopedTempDir temp_directory_;

  std::unique_ptr<PermissionAuditingService> service_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;
};

TEST_F(PermissionAuditingServiceTest, StorePermissionUsage) {
  std::vector<url::Origin> origins(std::size(kTestOrigins));
  std::vector<PermissionUsageSession> sessions(std::size(kTestOrigins));
  for (size_t i = 0; i < std::size(kTestOrigins); ++i) {
    origins[i] = url::Origin::Create(GURL(kTestOrigins[i]));
    sessions[i] = BuildUsageSession(kTestTypes[i % 2], origins[i]);
    service().StorePermissionUsage(sessions[i]);
  }
  for (size_t i = 0; i < std::size(kTestOrigins); ++i) {
    EXPECT_THAT(
        GetPermissionUsageHistory(kTestTypes[i % 2], origins[i], base::Time()),
        ElementsAre(sessions[i]));
  }
}

TEST_F(PermissionAuditingServiceTest, GetLastPermissionUsageTime) {
  std::vector<url::Origin> origins(std::size(kTestOrigins));
  for (size_t i = 0; i < std::size(kTestOrigins); ++i) {
    origins[i] = url::Origin::Create(GURL(kTestOrigins[i]));
    service().StorePermissionUsage(
        BuildUsageSession(kTestTypes[0], origins[i]));
    task_environment().FastForwardBy(base::TimeDelta());
    EXPECT_EQ(GetLastPermissionUsageTime(kTestTypes[0], origins[i]),
              kTestTimes[1]);
  }
}

TEST_F(PermissionAuditingServiceTest, UpdateEndTime) {
  std::vector<url::Origin> origins(std::size(kTestOrigins));
  for (size_t i = 0; i < std::size(kTestOrigins); ++i) {
    origins[i] = url::Origin::Create(GURL(kTestOrigins[i]));
    service().StorePermissionUsage(
        BuildUsageSession(kTestTypes[1], origins[i]));
  }
  for (const auto& origin : origins) {
    service().UpdateEndTime(kTestTypes[1], origin, kTestTimes[0],
                            kTestTimes[2]);
    EXPECT_EQ(GetLastPermissionUsageTime(kTestTypes[1], origin), kTestTimes[2]);
  }
}

TEST_F(PermissionAuditingServiceTest, DeleteSessionsBetween) {
  auto session1 = BuildUsageSession(kTestTimes[0], kTestTimes[1]);
  auto session2 = BuildUsageSession(kTestTimes[1], kTestTimes[2]);
  auto origin = url::Origin::Create(GURL(kTestOrigins[0]));
  auto type = session1.type;
  session1.origin = origin;
  session2.origin = origin;
  auto store_sessions = [&]() {
    service().StorePermissionUsage(session1);
    service().StorePermissionUsage(session2);
    task_environment().FastForwardBy(base::TimeDelta());
  };
  auto history = [&]() -> std::vector<PermissionUsageSession> {
    return GetPermissionUsageHistory(type, origin, base::Time());
  };
  store_sessions();
  ASSERT_THAT(history(), UnorderedElementsAre(session1, session2));
  service().DeleteSessionsBetween(kTestTimes[0], kTestTimes[1]);
  ASSERT_THAT(history(), IsEmpty());

  store_sessions();
  ASSERT_THAT(history(), UnorderedElementsAre(session1, session2));
  service().DeleteSessionsBetween(kTestTimes[1], kTestTimes[2]);
  ASSERT_THAT(history(), IsEmpty());

  store_sessions();
  ASSERT_THAT(history(), UnorderedElementsAre(session1, session2));
  service().DeleteSessionsBetween(kTestTimes[0], kTestTimes[2]);
  ASSERT_THAT(history(), IsEmpty());

  store_sessions();
  ASSERT_THAT(history(), UnorderedElementsAre(session1, session2));
  service().DeleteSessionsBetween(kTestTimes[0], kTestTimes[0]);
  ASSERT_THAT(history(), ElementsAre(session2));
  service().DeleteSessionsBetween(kTestTimes[1], kTestTimes[1]);
  ASSERT_THAT(history(), IsEmpty());

  store_sessions();
  ASSERT_THAT(history(), UnorderedElementsAre(session1, session2));
  service().DeleteSessionsBetween(kTestTimes[2], kTestTimes[2]);
  ASSERT_THAT(history(), ElementsAre(session1));
  service().DeleteSessionsBetween(kTestTimes[1], kTestTimes[1]);
  ASSERT_THAT(history(), IsEmpty());
}

TEST_F(PermissionAuditingServiceTest, OldSessionsAreExpired) {
  auto delay = service().GetUsageSessionCullingInterval();
  auto time1 = base::Time::Now() - service().GetUsageSessionMaxAge() + delay;
  auto time2 = time1 + 2 * delay;
  auto time3 = time2 + 2 * delay;
  auto session1 = BuildUsageSession(time1, time1);
  auto session2 = BuildUsageSession(time2, time2);
  auto session3 = BuildUsageSession(time3, time3);
  service().StorePermissionUsage(session1);
  service().StorePermissionUsage(session2);
  service().StorePermissionUsage(session3);
  auto history = [&]() -> std::vector<PermissionUsageSession> {
    return GetPermissionUsageHistory(kTestTypes[0],
                                     url::Origin::Create(GURL(kTestOrigins[0])),
                                     base::Time());
  };

  ASSERT_THAT(history(), UnorderedElementsAre(session1, session2, session3));
  task_environment().FastForwardBy(2 * delay);
  ASSERT_THAT(history(), UnorderedElementsAre(session2, session3));
  task_environment().FastForwardBy(2 * delay);
  ASSERT_THAT(history(), ElementsAre(session3));
  task_environment().FastForwardBy(2 * delay);
  EXPECT_THAT(history(), IsEmpty());
}

}  // namespace permissions
