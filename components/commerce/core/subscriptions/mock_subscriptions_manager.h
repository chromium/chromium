// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_MOCK_SUBSCRIPTIONS_MANAGER_H_
#define COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_MOCK_SUBSCRIPTIONS_MANAGER_H_

#include "components/commerce/core/subscriptions/subscriptions_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace commerce {

class MockSubscriptionsManager : public SubscriptionsManager {
 public:
  MockSubscriptionsManager();
  ~MockSubscriptionsManager() override;

  // SubscriptionsManager overrides:
  MOCK_METHOD(void,
              CheckTimestampOnBookmarkChange,
              (int64_t bookmark_subscription_change_time),
              (override));
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_MOCK_SUBSCRIPTIONS_MANAGER_H_
