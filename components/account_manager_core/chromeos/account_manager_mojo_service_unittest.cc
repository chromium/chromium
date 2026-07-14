// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/chromeos/account_manager_mojo_service.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/crosapi/mojom/account_manager.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_util.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/prefs/testing_pref_service.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {


// A test spy for intercepting AccountManager calls.
class AccountManagerSpy : public account_manager::AccountManager {
 public:
  AccountManagerSpy() = default;
  AccountManagerSpy(const AccountManagerSpy&) = delete;
  AccountManagerSpy& operator=(const AccountManagerSpy&) = delete;
  ~AccountManagerSpy() override = default;

  // account_manager::AccountManager override:
  std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const ::account_manager::AccountKey& account_key,
      OAuth2AccessTokenConsumer* consumer) override {
    num_access_token_fetches_++;
    last_access_token_account_key_ = account_key;

    return account_manager::AccountManager::CreateAccessTokenFetcher(
        account_key, consumer);
  }

  int num_access_token_fetches() const { return num_access_token_fetches_; }

  account_manager::AccountKey last_access_token_account_key() const {
    return last_access_token_account_key_.value();
  }

 private:
  // Mutated by const CreateAccessTokenFetcher.
  mutable int num_access_token_fetches_ = 0;
  mutable std::optional<account_manager::AccountKey>
      last_access_token_account_key_;
};

class AccountManagerMojoServiceTest : public ::testing::Test {
 public:
  AccountManagerMojoServiceTest() = default;
  AccountManagerMojoServiceTest(const AccountManagerMojoServiceTest&) = delete;
  AccountManagerMojoServiceTest& operator=(
      const AccountManagerMojoServiceTest&) = delete;
  ~AccountManagerMojoServiceTest() override = default;

 protected:
  void SetUp() override {
    account_manager_mojo_service_ =
        std::make_unique<AccountManagerMojoService>(&account_manager_);
    account_manager_mojo_service_->BindReceiver(
        remote_.BindNewPipeAndPassReceiver());
    account_manager_async_waiter_ =
        std::make_unique<mojom::AccountManagerAsyncWaiter>(
            account_manager_mojo_service_.get());
  }

  void RunAllPendingTasks() { task_environment_.RunUntilIdle(); }

  // Returns `true` if initialization was successful.
  bool InitializeAccountManager() {
    base::test::TestFuture<void> future;
    account_manager_.InitializeInEphemeralMode(
        test_url_loader_factory_.GetSafeWeakWrapper(), future.GetCallback());
    account_manager_.SetPrefService(&pref_service_);
    account_manager_.RegisterPrefs(pref_service_.registry());
    EXPECT_TRUE(future.Wait());
    return account_manager_.IsInitialized();
  }

  mojom::AccountManagerAsyncWaiter* account_manager_async_waiter() {
    return account_manager_async_waiter_.get();
  }

  AccountManagerSpy* account_manager() { return &account_manager_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  TestingPrefServiceSimple pref_service_;
  AccountManagerSpy account_manager_;
  mojo::Remote<mojom::AccountManager> remote_;
  std::unique_ptr<AccountManagerMojoService> account_manager_mojo_service_;
  std::unique_ptr<mojom::AccountManagerAsyncWaiter>
      account_manager_async_waiter_;
};

}  // namespace crosapi
