// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache_wrapper.h"
#include "components/services/app_service/public/cpp/publisher_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

class AppCapabilityAccessCacheWrapperTest
    : public testing::Test,
      public AppCapabilityAccessCache::Observer {
 protected:
  // apps::AppCapabilityAccessCache::Observer:
  void OnCapabilityAccessUpdate(const CapabilityAccessUpdate& update) override {
    last_account_id_ = update.AccountId();
  }

  void OnAppCapabilityAccessCacheWillBeDestroyed(
      apps::AppCapabilityAccessCache* cache) override {
    NOTREACHED();
  }

  static mojom::CapabilityAccessPtr MakeCapabilityAccess(
      const char* app_id,
      mojom::OptionalBool camera,
      mojom::OptionalBool microphone) {
    mojom::CapabilityAccessPtr access = mojom::CapabilityAccess::New();
    access->app_id = app_id;
    access->camera = camera;
    access->microphone = microphone;
    return access;
  }

  void VerifyAccountId(AccountId account_id) {
    EXPECT_EQ(account_id, last_account_id_);
  }

  AccountId& account_id_1() { return account_id_1_; }
  AccountId& account_id_2() { return account_id_2_; }

 private:
  AccountId last_account_id_;
  AccountId account_id_1_ = AccountId::FromUserEmail("fake_email@gmail.com");
  AccountId account_id_2_ = AccountId::FromUserEmail("fake_email2@gmail.com");
};

TEST_F(AppCapabilityAccessCacheWrapperTest, OneAccount) {
  AppCapabilityAccessCache cache1;
  cache1.SetAccountId(account_id_1());

  AppCapabilityAccessCacheWrapper::Get().AddAppCapabilityAccessCache(
      account_id_1(), &cache1);

  cache1.AddObserver(this);

  std::vector<apps::mojom::CapabilityAccessPtr> deltas;
  deltas.push_back(MakeCapabilityAccess("a", mojom::OptionalBool::kTrue,
                                        mojom::OptionalBool::kTrue));
  cache1.OnCapabilityAccesses(std::move(deltas));

  VerifyAccountId(account_id_1());
  cache1.RemoveObserver(this);
}

TEST_F(AppCapabilityAccessCacheWrapperTest, MultipleAccounts) {
  AppCapabilityAccessCache cache1;
  AppCapabilityAccessCache cache2;
  cache1.SetAccountId(account_id_1());
  cache2.SetAccountId(account_id_2());

  AppCapabilityAccessCacheWrapper::Get().AddAppCapabilityAccessCache(
      account_id_1(), &cache1);
  AppCapabilityAccessCacheWrapper::Get().AddAppCapabilityAccessCache(
      account_id_1(), &cache2);

  cache1.AddObserver(this);

  std::vector<apps::mojom::CapabilityAccessPtr> deltas;
  deltas.push_back(MakeCapabilityAccess("a", mojom::OptionalBool::kTrue,
                                        mojom::OptionalBool::kTrue));
  cache1.OnCapabilityAccesses(std::move(deltas));

  VerifyAccountId(account_id_1());
  cache1.RemoveObserver(this);

  cache2.AddObserver(this);
  deltas.clear();
  deltas.push_back(MakeCapabilityAccess("a", mojom::OptionalBool::kTrue,
                                        mojom::OptionalBool::kTrue));
  cache2.OnCapabilityAccesses(std::move(deltas));

  VerifyAccountId(account_id_2());
  cache2.RemoveObserver(this);

  AppCapabilityAccessCacheWrapper::Get().RemoveAppCapabilityAccessCache(
      &cache2);
  EXPECT_FALSE(
      AppCapabilityAccessCacheWrapper::Get().GetAppCapabilityAccessCache(
          account_id_2()));
}

}  // namespace apps
