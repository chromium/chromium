// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"

#include <limits>
#include <memory>

#include "base/no_destructor.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/account_manager_facade_impl.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/account_manager_core/chromeos/account_manager_mojo_service.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace {

mojo::Remote<crosapi::mojom::AccountManager> GetAccountManagerRemote() {
  mojo::Remote<crosapi::mojom::AccountManager> remote;

  auto* lacros_chrome_service_impl = chromeos::LacrosService::Get();
  DCHECK(lacros_chrome_service_impl);
  if (!lacros_chrome_service_impl->IsAccountManagerAvailable()) {
    LOG(WARNING) << "Connected to an older version of ash. Account "
                    "consistency will not be available";
    return remote;
  }

  lacros_chrome_service_impl->BindAccountManagerReceiver(
      remote.BindNewPipeAndPassReceiver());

  return remote;
}

class AccountManagerFacadeFactoryLacros {
 public:
  void CreateAshAccountManagerForTests() {
    Reset();
    ash_account_manager_ = std::make_unique<account_manager::AccountManager>();
    account_manager_mojo_service_ =
        std::make_unique<crosapi::AccountManagerMojoService>(
            ash_account_manager_.get());
  }

  account_manager::AccountManagerFacadeImpl* GetAccountManagerFacade() {
    if (!account_manager_facade_) {
      InitializeAccountManagerFacade();
    }
    return account_manager_facade_.get();
  }

  account_manager::AccountManager* MaybeGetAshAccountManagerForTests() {
    return ash_account_manager_.get();
  }

 private:
  void InitializeAccountManagerFacade() {
    if (ash_account_manager_) {
      // Create a new `AccountManagerFacade` that is connected to
      // `AccountManagerMojoService` for tests.
      mojo::Remote<crosapi::mojom::AccountManager> remote;
      account_manager_mojo_service_->BindReceiver(
          remote.BindNewPipeAndPassReceiver());

      account_manager_facade_ =
          std::make_unique<account_manager::AccountManagerFacadeImpl>(
              std::move(remote),
              /*remote_version=*/std::numeric_limits<uint32_t>::max());
      return;
    }

    account_manager_facade_ =
        std::make_unique<account_manager::AccountManagerFacadeImpl>(
            GetAccountManagerRemote(),
            /*remote_version=*/chromeos::LacrosService::Get()
                ->GetInterfaceVersion(crosapi::mojom::AccountManager::Uuid_));
  }

  // Reset the pointers.
  void Reset() {
    // AccountManagerFacade depends on AccountManagerMojoService.
    account_manager_facade_.reset();
    // AccountManagerMojoService depends on AccountManager.
    account_manager_mojo_service_.reset();
    ash_account_manager_.reset();
  }

  std::unique_ptr<account_manager::AccountManagerFacadeImpl>
      account_manager_facade_;
  // Set only in tests:
  std::unique_ptr<account_manager::AccountManager> ash_account_manager_;
  std::unique_ptr<crosapi::AccountManagerMojoService>
      account_manager_mojo_service_;
};

AccountManagerFacadeFactoryLacros* GetAccountManagerFacadeFactoryLacros() {
  static base::NoDestructor<AccountManagerFacadeFactoryLacros> factory;
  return factory.get();
}

}  // namespace

account_manager::AccountManagerFacade* GetAccountManagerFacade(
    const std::string& profile_path) {
  // Multi-Login is disabled with Lacros. Always return the same instance.
  return GetAccountManagerFacadeFactoryLacros()->GetAccountManagerFacade();
}

void CreateAshAccountManagerForTests() {
  GetAccountManagerFacadeFactoryLacros()
      ->CreateAshAccountManagerForTests();  // IN-TEST
}

account_manager::AccountManager* MaybeGetAshAccountManagerForTests() {
  return GetAccountManagerFacadeFactoryLacros()
      ->MaybeGetAshAccountManagerForTests();  // IN-TEST
}
