// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/version_info/channel.h"
#include "components/activity_reporter/activity_reporter.h"
#include "components/activity_reporter/activity_reporter_for_testing.h"
#include "components/activity_reporter/constants.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/network.h"
#include "components/update_client/update_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace activity_reporter {

namespace {

using ::testing::_;

class MockUpdateClient : public update_client::UpdateClient {
 public:
  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));
  MOCK_METHOD(base::RepeatingClosure,
              Install,
              (const std::string& id,
               CrxDataCallback crx_data_callback,
               CrxStateChangeCallback crx_state_change_callback,
               update_client::Callback callback),
              (override));
  MOCK_METHOD(void,
              Update,
              (const std::vector<std::string>& ids,
               CrxDataCallback crx_data_callback,
               CrxStateChangeCallback crx_state_change_callback,
               bool is_foreground,
               update_client::Callback callback),
              (override));
  MOCK_METHOD(void,
              CheckForUpdate,
              (const std::string& id,
               CrxDataCallback crx_data_callback,
               CrxStateChangeCallback crx_state_change_callback,
               bool is_foreground,
               update_client::Callback callback),
              (override));
  MOCK_METHOD(bool,
              GetCrxUpdateState,
              (const std::string& id,
               update_client::CrxUpdateItem* update_item),
              (const, override));
  MOCK_METHOD(bool, IsUpdating, (const std::string& id), (const, override));
  MOCK_METHOD(void, Stop, (), (override));
  MOCK_METHOD(void,
              SendPing,
              (const update_client::CrxComponent& crx_component,
               PingParams ping_params,
               update_client::Callback callback),
              (override));
  MOCK_METHOD(void,
              CleanupStaleDownloads,
              (base::Time older_than, base::OnceClosure callback),
              (override));

 private:
  ~MockUpdateClient() override = default;
};

}  // namespace

class ActivityReporterImplTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<MockUpdateClient> mock_update_client_ =
      base::MakeRefCounted<MockUpdateClient>();
  std::unique_ptr<ActivityReporter> activity_reporter_ =
      CreateActivityReporterForTesting(
          mock_update_client_,
          base::DoNothing(),
          base::BindRepeating([] { return version_info::Channel::UNKNOWN; }));
};

TEST_F(ActivityReporterImplTest, ReportActive_Throttling) {
  int call_count = 0;
  EXPECT_CALL(*mock_update_client_, CheckForUpdate(_, _, _, _, _))
      .Times(2)
      .WillRepeatedly(
          [&](const std::string& id,
              update_client::UpdateClient::CrxDataCallback crx_data_callback,
              update_client::UpdateClient::CrxStateChangeCallback, bool,
              update_client::Callback) {
            ++call_count;
            EXPECT_EQ(id, kChromeActivityId);
            std::move(crx_data_callback)
                .Run({std::string{kChromeActivityId}}, base::DoNothing());
          });

  // The first report should go through.
  activity_reporter_->ReportActive();
  EXPECT_EQ(call_count, 1);

  // The second report should be throttled.
  activity_reporter_->ReportActive();
  ASSERT_EQ(call_count, 1);

  // Advance the clock by 5 hours. The third report should go through.
  task_environment_.FastForwardBy(base::Hours(5));
  activity_reporter_->ReportActive();
  EXPECT_EQ(call_count, 2);
}

}  // namespace activity_reporter
