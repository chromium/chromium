// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/scoped_observation.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

class AppRegistryCacheWrapperTest : public testing::Test,
                                    public AppRegistryCache::Observer {
 protected:
  // AppRegistryCache::Observer:
  void OnAppUpdate(const AppUpdate& update) override {
    last_account_id_ = update.AccountId();
  }

  void OnAppRegistryCacheWillBeDestroyed(AppRegistryCache* cache) override {
    NOTREACHED_IN_MIGRATION();
  }

  void VerifyAccountId(AccountId account_id) {
    EXPECT_EQ(account_id, last_account_id_);
  }

  void OnApps(AppRegistryCache& cache,
              std::vector<AppPtr> deltas,
              apps::AppType app_type,
              bool should_notify_initialized) {
    cache.OnApps(std::move(deltas), app_type, should_notify_initialized);
  }

  AccountId& account_id_1() { return account_id_1_; }
  AccountId& account_id_2() { return account_id_2_; }

 private:
  AccountId last_account_id_;
  AccountId account_id_1_ = AccountId::FromUserEmail("fake_email@gmail.com");
  AccountId account_id_2_ = AccountId::FromUserEmail("fake_email2@gmail.com");
};

TEST_F(AppRegistryCacheWrapperTest, OneAccount) {
  AppRegistryCache cache1;
  cache1.SetAccountId(account_id_1());

  AppRegistryCacheWrapper::Get().AddAppRegistryCache(account_id_1(), &cache1);

  base::ScopedObservation<AppRegistryCache, AppRegistryCache::Observer>
      observation{this};
  observation.Observe(&cache1);

  std::vector<AppPtr> deltas;
  deltas.push_back(std::make_unique<App>(AppType::kArc, "app_id"));
  OnApps(cache1, std::move(deltas), AppType::kArc,
         /*should_notify_initialized=*/true);

  VerifyAccountId(account_id_1());
  observation.Reset();
}

TEST_F(AppRegistryCacheWrapperTest, MultipleAccounts) {
  AppRegistryCache cache1;
  AppRegistryCache cache2;
  cache1.SetAccountId(account_id_1());
  cache2.SetAccountId(account_id_2());

  AppRegistryCacheWrapper::Get().AddAppRegistryCache(account_id_1(), &cache1);
  AppRegistryCacheWrapper::Get().AddAppRegistryCache(account_id_2(), &cache2);

  base::ScopedObservation<AppRegistryCache, AppRegistryCache::Observer>
      observation{this};
  observation.Observe(&cache1);

  std::vector<AppPtr> deltas1;
  deltas1.push_back(std::make_unique<App>(AppType::kArc, "app_id1"));
  OnApps(cache1, std::move(deltas1), AppType::kArc,
         /*should_notify_initialized=*/true);

  VerifyAccountId(account_id_1());

  observation.Reset();
  observation.Observe(&cache2);

  std::vector<AppPtr> deltas2;
  deltas2.push_back(std::make_unique<App>(AppType::kArc, "app_id2"));
  OnApps(cache2, std::move(deltas2), AppType::kArc,
         /*should_notify_initialized=*/true);

  VerifyAccountId(account_id_2());
  observation.Reset();

  AppRegistryCacheWrapper::Get().RemoveAppRegistryCache(&cache2);
  EXPECT_FALSE(
      AppRegistryCacheWrapper::Get().GetAppRegistryCache(account_id_2()));
}

TEST_F(AppRegistryCacheWrapperTest, RegistryCacheRemovedIfFreed) {
  auto cache = std::make_unique<AppRegistryCache>();
  cache->SetAccountId(account_id_1());

  AppRegistryCacheWrapper::Get().AddAppRegistryCache(account_id_1(),
                                                     cache.get());

  cache.reset();

  EXPECT_EQ(AppRegistryCacheWrapper::Get().GetAppRegistryCache(account_id_1()),
            nullptr);
}

}  // namespace apps
