// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/services/app_service/public/cpp/publisher_base.h"
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
    NOTREACHED();
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

TEST_F(AppRegistryCacheWrapperTest, OneAccount) {
  AppRegistryCache cache1;
  cache1.SetAccountId(account_id_1());

  AppRegistryCacheWrapper::Get().AddAppRegistryCache(account_id_1(), &cache1);

  cache1.AddObserver(this);

  std::vector<AppPtr> deltas;
  deltas.push_back(std::make_unique<App>(AppType::kArc, "app_id"));
  cache1.OnApps(std::move(deltas), AppType::kArc,
                true /* should_notify_initialized */);

  // TODO(crbug.com/1253250): Remove after migrating to non-mojo AppService.
  std::vector<apps::mojom::AppPtr> mojom_deltas;
  mojom_deltas.push_back(PublisherBase::MakeApp(
      apps::mojom::AppType::kArc, "app_id", apps::mojom::Readiness::kUnknown,
      "name", apps::mojom::InstallReason::kDefault));
  cache1.OnApps(std::move(mojom_deltas), apps::mojom::AppType::kArc,
                true /* should_notify_initialized */);

  VerifyAccountId(account_id_1());
  cache1.RemoveObserver(this);
}

TEST_F(AppRegistryCacheWrapperTest, MultipleAccounts) {
  AppRegistryCache cache1;
  AppRegistryCache cache2;
  cache1.SetAccountId(account_id_1());
  cache2.SetAccountId(account_id_2());

  AppRegistryCacheWrapper::Get().AddAppRegistryCache(account_id_1(), &cache1);
  AppRegistryCacheWrapper::Get().AddAppRegistryCache(account_id_2(), &cache2);

  cache1.AddObserver(this);

  std::vector<AppPtr> deltas1;
  deltas1.push_back(std::make_unique<App>(AppType::kArc, "app_id1"));
  cache1.OnApps(std::move(deltas1), AppType::kArc,
                /*should_notify_initialized=*/true);

  // TODO(crbug.com/1253250): Remove after migrating to non-mojo AppService.
  std::vector<apps::mojom::AppPtr> mojom_deltas;
  mojom_deltas.push_back(PublisherBase::MakeApp(
      apps::mojom::AppType::kArc, "app_id", apps::mojom::Readiness::kUnknown,
      "name", apps::mojom::InstallReason::kDefault));
  cache1.OnApps(std::move(mojom_deltas), apps::mojom::AppType::kArc,
                true /* should_notify_initialized */);

  VerifyAccountId(account_id_1());
  cache1.RemoveObserver(this);

  cache2.AddObserver(this);

  std::vector<AppPtr> deltas2;
  deltas2.push_back(std::make_unique<App>(AppType::kArc, "app_id2"));
  cache2.OnApps(std::move(deltas2), AppType::kArc,
                /*should_notify_initialized=*/true);

  // TODO(crbug.com/1253250): Remove after migrating to non-mojo AppService.
  mojom_deltas.clear();
  mojom_deltas.push_back(PublisherBase::MakeApp(
      apps::mojom::AppType::kArc, "app_id2", apps::mojom::Readiness::kUnknown,
      "name", apps::mojom::InstallReason::kDefault));
  cache2.OnApps(std::move(mojom_deltas), apps::mojom::AppType::kArc,
                true /* should_notify_initialized */);

  VerifyAccountId(account_id_2());
  cache2.RemoveObserver(this);

  AppRegistryCacheWrapper::Get().RemoveAppRegistryCache(&cache2);
  EXPECT_FALSE(
      AppRegistryCacheWrapper::Get().GetAppRegistryCache(account_id_2()));
}

}  // namespace apps
