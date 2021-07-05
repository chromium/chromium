// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accuracy_tips/accuracy_service.h"
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/accuracy_tips/accuracy_tip_status.h"
#include "components/accuracy_tips/features.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/test_database_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Return;

namespace accuracy_tips {

class MockAccuracyTipUI : public AccuracyTipUI {
 public:
  MOCK_METHOD3(ShowAccuracyTip,
               void(content::WebContents*,
                    AccuracyTipStatus,
                    base::OnceCallback<void(Interaction)>));
};

class MockSafeBrowsingDatabaseManager
    : public safe_browsing::TestSafeBrowsingDatabaseManager {
 public:
  MockSafeBrowsingDatabaseManager()
      : TestSafeBrowsingDatabaseManager(base::ThreadTaskRunnerHandle::Get(),
                                        base::ThreadTaskRunnerHandle::Get()) {}

  MOCK_METHOD2(CheckUrlForAccuracyTips, bool(const GURL&, Client*));

 protected:
  ~MockSafeBrowsingDatabaseManager() override = default;
};

// Handler to mark URLs as part of the AccuracyTips list.
bool IsOnList(const GURL& url,
              safe_browsing::SafeBrowsingDatabaseManager::Client* client) {
  client->OnCheckUrlForAccuracyTip(true);
  return false;
}

// Handler to simulate URLs that match the local hash but are not on the list.
bool IsLocalMatchButNotOnList(
    const GURL& url,
    safe_browsing::SafeBrowsingDatabaseManager::Client* client) {
  client->OnCheckUrlForAccuracyTip(false);
  return false;
}

class AccuracyServiceTest : public ::testing::Test {
 protected:
  AccuracyServiceTest() = default;

  void SetUp() override {
    feature_list.InitAndEnableFeatureWithParameters(
        safe_browsing::kAccuracyTipsFeature,
        {{kSampleUrl.name, "https://sampleurl.com"}});

    auto ui = std::make_unique<testing::StrictMock<MockAccuracyTipUI>>();
    ui_ = ui.get();
    sb_database_ = base::MakeRefCounted<MockSafeBrowsingDatabaseManager>();
    service_ = std::make_unique<AccuracyService>(
        std::move(ui), sb_database_, base::ThreadTaskRunnerHandle::Get(),
        base::ThreadTaskRunnerHandle::Get());
  }

  AccuracyService* service() { return service_.get(); }
  MockAccuracyTipUI* ui() { return ui_; }
  MockSafeBrowsingDatabaseManager* sb_database() { return sb_database_.get(); }

 private:
  base::test::SingleThreadTaskEnvironment environment;
  base::test::ScopedFeatureList feature_list;

  std::unique_ptr<AccuracyService> service_;
  MockAccuracyTipUI* ui_;
  scoped_refptr<MockSafeBrowsingDatabaseManager> sb_database_;
};

TEST_F(AccuracyServiceTest, CheckAccuracyStatusForRandomSite) {
  auto url = GURL("https://example.com");
  base::MockOnceCallback<void(AccuracyTipStatus)> callback;
  EXPECT_CALL(callback, Run(AccuracyTipStatus::kNone));
  EXPECT_CALL(*sb_database(), CheckUrlForAccuracyTips(url, _))
      .WillOnce(Return(true));

  service()->CheckAccuracyStatus(url, callback.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(AccuracyServiceTest, CheckAccuracyStatusForSampleUrl) {
  auto url = GURL("https://sampleurl.com");
  base::MockOnceCallback<void(AccuracyTipStatus)> callback;
  EXPECT_CALL(callback, Run(AccuracyTipStatus::kShowAccuracyTip));

  service()->CheckAccuracyStatus(url, callback.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(AccuracyServiceTest, CheckAccuracyStatusForUrlOnList) {
  auto url = GURL("https://badurl.com");
  base::MockOnceCallback<void(AccuracyTipStatus)> callback;
  EXPECT_CALL(callback, Run(AccuracyTipStatus::kShowAccuracyTip));
  EXPECT_CALL(*sb_database(), CheckUrlForAccuracyTips(url, _))
      .WillOnce(Invoke(&IsOnList));

  service()->CheckAccuracyStatus(url, callback.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(AccuracyServiceTest, CheckAccuracyStatusForLocalMatch) {
  auto url = GURL("https://notactuallybadurl.com");
  base::MockOnceCallback<void(AccuracyTipStatus)> callback;
  EXPECT_CALL(callback, Run(AccuracyTipStatus::kNone));
  EXPECT_CALL(*sb_database(), CheckUrlForAccuracyTips(url, _))
      .WillOnce(Invoke(&IsLocalMatchButNotOnList));

  service()->CheckAccuracyStatus(url, callback.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(AccuracyServiceTest, ShowUI) {
  EXPECT_CALL(*ui(), ShowAccuracyTip(_, _, _));
  service()->MaybeShowAccuracyTip(nullptr);
}

}  // namespace accuracy_tips