// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_MOCK_SMART_BUBBLE_STATS_STORE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_MOCK_SMART_BUBBLE_STATS_STORE_H_

#include "base/functional/callback.h"
#include "components/password_manager/core/browser/password_store/smart_bubble_stats_store.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

class MockSmartBubbleStatsStore : public SmartBubbleStatsStore {
 public:
  MockSmartBubbleStatsStore();
  ~MockSmartBubbleStatsStore() override;

  MOCK_METHOD(void, AddSiteStats, (const InteractionsStats&), (override));
  MOCK_METHOD(void, RemoveSiteStats, (const GURL&), (override));
  MOCK_METHOD(void,
              GetSiteStats,
              (const GURL&, base::WeakPtr<PasswordStoreConsumer>),
              (override));
  MOCK_METHOD(void,
              RemoveStatisticsByOriginAndTime,
              (const base::RepeatingCallback<bool(const GURL&)>&,
               base::Time,
               base::Time,
               base::OnceClosure),
              (override));
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_MOCK_SMART_BUBBLE_STATS_STORE_H_
