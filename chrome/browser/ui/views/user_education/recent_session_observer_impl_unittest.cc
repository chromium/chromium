// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/recent_session_observer_impl.h"

#include <memory>
#include <utility>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/user_education/recent_session_policy.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/user_education/browser_feature_promo_storage_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/expect_call_in_scope.h"

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

class RecentSessionObserverImplTest : public TestWithBrowserView {
 public:
  RecentSessionObserverImplTest() = default;
  ~RecentSessionObserverImplTest() override = default;

  static void SendUpdate(RecentSessionObserverImpl& observer,
                         const RecentSessionData& data) {
    observer.OnRecentSessionsUpdated(data);
  }
};

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
  SendUpdate(*observer, data);
}

TEST_F(RecentSessionObserverImplTest, SessionCallback) {
  UNCALLED_MOCK_CALLBACK(base::RepeatingClosure, callback);
  auto policy_ptr =
      std::make_unique<testing::StrictMock<MockRecentSessionPolicy>>();
  auto& policy = *policy_ptr;
  const auto observer = std::make_unique<RecentSessionObserverImpl>(
      *browser_view()->GetProfile(), std::move(policy_ptr));
  const auto subscription =
      observer->AddLowUsageSessionCallback(callback.Get());

  RecentSessionData data;

  // If should enable is false, no callback is sent.
  EXPECT_CALL(policy, RecordRecentUsageMetrics(testing::Ref(data))).Times(1);
  EXPECT_CALL(policy, ShouldEnableLowUsagePromoMode(testing::Ref(data)))
      .WillOnce(testing::Return(false));
  SendUpdate(*observer, data);

  // If should enable is true, callback is sent.
  EXPECT_CALL(policy, RecordRecentUsageMetrics(testing::Ref(data))).Times(1);
  EXPECT_CALL(policy, ShouldEnableLowUsagePromoMode(testing::Ref(data)))
      .WillOnce(testing::Return(true));
  EXPECT_CALL_IN_SCOPE(callback, Run, SendUpdate(*observer, data));
}

TEST_F(RecentSessionObserverImplTest, SessionCallbackOnObserve) {
  UNCALLED_MOCK_CALLBACK(base::RepeatingClosure, callback);
  auto policy_ptr =
      std::make_unique<testing::StrictMock<MockRecentSessionPolicy>>();
  auto& policy = *policy_ptr;
  const auto observer = std::make_unique<RecentSessionObserverImpl>(
      *browser_view()->GetProfile(), std::move(policy_ptr));

  RecentSessionData data;

  EXPECT_CALL(policy, RecordRecentUsageMetrics(testing::Ref(data))).Times(1);
  EXPECT_CALL(policy, ShouldEnableLowUsagePromoMode(testing::Ref(data)))
      .WillOnce(testing::Return(true));
  SendUpdate(*observer, data);

  // Already have a triggering session, so callback is sent on subscribe.
  base::CallbackListSubscription subscription;
  EXPECT_CALL_IN_SCOPE(
      callback, Run,
      subscription = observer->AddLowUsageSessionCallback(callback.Get()));

  // Now send another and ensure that the callback is called again.
  EXPECT_CALL(policy, RecordRecentUsageMetrics(testing::Ref(data))).Times(1);
  EXPECT_CALL(policy, ShouldEnableLowUsagePromoMode(testing::Ref(data)))
      .WillOnce(testing::Return(true));
  EXPECT_CALL_IN_SCOPE(callback, Run, SendUpdate(*observer, data));
}
