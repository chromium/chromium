// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/activity_reporter/configurator.h"

#include <set>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/version_info/channel.h"
#include "components/activity_reporter/constants.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/network.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/update_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace activity_reporter {

namespace {

using ::testing::UnorderedElementsAre;

// An app id other than `kChromeActivityId`.
constexpr char kOtherAppId[] = "other_app";

}  // namespace

class ActivityReporterConfiguratorTest : public testing::Test {
 protected:
  ActivityReporterConfiguratorTest() {
    update_client::RegisterPrefs(pref_service_.registry());
    configurator_ = base::MakeRefCounted<ActivityReporterConfigurator>(
        base::BindRepeating([](PrefService* prefs) { return prefs; },
                            &pref_service_),
        /*network_fetcher_factory=*/nullptr,
        base::BindRepeating([] { return version_info::Channel::UNKNOWN; }),
        /*per_user_install=*/false);
  }

  update_client::PersistedData* persisted_data() {
    return configurator_->GetPersistedData();
  }

  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  scoped_refptr<ActivityReporterConfigurator> configurator_;
};

TEST_F(ActivityReporterConfiguratorTest, ReportsRequestedAppsActive) {
  base::test::TestFuture<const std::set<std::string>&> active_ids;
  persisted_data()->GetActiveBits({std::string(kChromeActivityId), kOtherAppId},
                                  active_ids.GetCallback());
  EXPECT_THAT(active_ids.Get(),
              UnorderedElementsAre(kChromeActivityId, kOtherAppId));
}

TEST_F(ActivityReporterConfiguratorTest, WritesDateLastActiveOfRequestedApp) {
  base::test::TestFuture<void> done;
  persisted_data()->SetDateLastData({kOtherAppId}, 7100, done.GetCallback());
  ASSERT_TRUE(done.Wait());
  EXPECT_EQ(persisted_data()->GetDateLastActive(kOtherAppId), 7100);
}

}  // namespace activity_reporter
