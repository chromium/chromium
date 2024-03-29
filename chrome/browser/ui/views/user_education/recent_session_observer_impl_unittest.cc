// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/recent_session_observer_impl.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/user_education/recent_session_policy.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/user_education/browser_feature_promo_storage_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockRecentSessionPolicy : public RecentSessionPolicy {
 public:
  MockRecentSessionPolicy() = default;
  ~MockRecentSessionPolicy() override = default;

  MOCK_METHOD(void,
              RecordRecentUsageMetrics,
              (const RecentSessionData&),
              (override));
  MOCK_METHOD(bool,
              ShouldEnableLowUsagePromoMode,
              (const RecentSessionData&),
              (const, override));
};

}  // namespace

using RecentSessionObserverImplTest = TestWithBrowserView;

TEST_F(RecentSessionObserverImplTest, OnRecentSessionsUpdated) {
  auto policy_ptr =
      std::make_unique<testing::StrictMock<MockRecentSessionPolicy>>();
  auto& policy = *policy_ptr;
  const auto observer = std::make_unique<RecentSessionObserverImpl>(
      *browser_view()->GetProfile(), std::move(policy_ptr));

  RecentSessionData data;

  EXPECT_CALL(policy, RecordRecentUsageMetrics(testing::Ref(data))).Times(1);
  EXPECT_CALL(policy, ShouldEnableLowUsagePromoMode(testing::Ref(data)))
      .WillOnce(testing::Return(true));
  observer->OnRecentSessionsUpdated(data);
}
