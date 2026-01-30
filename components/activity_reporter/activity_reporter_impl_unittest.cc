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
  MOCK_METHOD1(AddObserver, void(Observer* observer));
  MOCK_METHOD1(RemoveObserver, void(Observer* observer));
  MOCK_METHOD4(
      Install,
      base::RepeatingClosure(const std::string& id,
                             CrxDataCallback crx_data_callback,
                             CrxStateChangeCallback crx_state_change_callback,
                             update_client::Callback callback));
  MOCK_METHOD5(Update,
               void(const std::vector<std::string>& ids,
                    CrxDataCallback crx_data_callback,
                    CrxStateChangeCallback crx_state_change_callback,
                    bool is_foreground,
                    update_client::Callback callback));
  MOCK_METHOD5(CheckForUpdate,
               void(const std::string& id,
                    CrxDataCallback crx_data_callback,
                    CrxStateChangeCallback crx_state_change_callback,
                    bool is_foreground,
                    update_client::Callback callback));
  MOCK_CONST_METHOD2(GetCrxUpdateState,
                     bool(const std::string& id,
                          update_client::CrxUpdateItem* update_item));
  MOCK_CONST_METHOD1(IsUpdating, bool(const std::string& id));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD3(SendPing,
               void(const update_client::CrxComponent& crx_component,
                    PingParams ping_params,
                    update_client::Callback callback));
  MOCK_METHOD2(SendRegistrationPing,
               void(const update_client::CrxComponent& crx_component,
                    update_client::Callback callback));

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
