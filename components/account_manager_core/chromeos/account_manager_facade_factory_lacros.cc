// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
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
  if (!lacros_chrome_service_impl
           ->IsSupported<crosapi::mojom::AccountManager>()) {
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
  void CreateAshAccountManagerForTests(
      std::unique_ptr<account_manager::AccountManagerUI> account_manager_ui) {
    // `account_manager_facade_` depends on `ash_account_manager_` and cannot be
    // reset, as users may be holding weak references to it.
    DCHECK(!account_manager_facade_);

    Reset();
    ash_account_manager_ = std::make_unique<account_manager::AccountManager>();
    account_manager_mojo_service_ =
        std::make_unique<crosapi::AccountManagerMojoService>(
            ash_account_manager_.get());
    account_manager_ui_ = account_manager_ui.get();
    account_manager_mojo_service_->SetAccountManagerUI(
        std::move(account_manager_ui));
  }

  account_manager::AccountManagerFacade* GetAccountManagerFacade() {
    if (!account_manager_facade_) {
      InitializeAccountManagerFacade();
    }
    return account_manager_facade_.get();
  }

  account_manager::AccountManager* MaybeGetAshAccountManagerForTests() {
    return ash_account_manager_.get();
  }

  account_manager::AccountManagerUI* MaybeGetAshAccountManagerUIForTests() {
    return account_manager_ui_;
  }

  crosapi::AccountManagerMojoService*
  MaybeGetAshAccountManagerMojoServiceForTests() {
    return account_manager_mojo_service_.get();
  }

  // Reset the pointers.
  void Reset() {
    // AccountManagerFacade depends on AccountManagerMojoService.
    account_manager_facade_.reset();
    account_manager_ui_ = nullptr;
    // AccountManagerMojoService depends on AccountManager.
    account_manager_mojo_service_.reset();
    ash_account_manager_.reset();
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
              /*remote_version=*/std::numeric_limits<uint32_t>::max(),
              /*account_manager_for_tests=*/ash_account_manager_->GetWeakPtr());
      return;
    }

    account_manager_facade_ =
        std::make_unique<account_manager::AccountManagerFacadeImpl>(
            GetAccountManagerRemote(),
            /*remote_version=*/
            chromeos::LacrosService::Get()
                ->GetInterfaceVersion<crosapi::mojom::AccountManager>(),
            /*account_manager_for_tests=*/nullptr);
  }

  std::unique_ptr<account_manager::AccountManagerFacade>
      account_manager_facade_;

  // Set only in tests:
  std::unique_ptr<account_manager::AccountManager> ash_account_manager_;
  std::unique_ptr<crosapi::AccountManagerMojoService>
      account_manager_mojo_service_;
  // Owned by `account_manager_mojo_service_`:
  raw_ptr<account_manager::AccountManagerUI> account_manager_ui_ = nullptr;
};

AccountManagerFacadeFactoryLacros* GetAccountManagerFacadeFactoryLacros() {
  static base::NoDestructor<AccountManagerFacadeFactoryLacros> factory;
  return factory.get();
}

}  // namespace

ScopedAshAccountManagerForTests::ScopedAshAccountManagerForTests(
    std::unique_ptr<account_manager::AccountManagerUI> account_manager_ui) {
  DCHECK(!MaybeGetAshAccountManagerForTests())  // IN-TEST
      << "Nested ScopedAshAccountManagerForTests are not supported.";
  GetAccountManagerFacadeFactoryLacros()->CreateAshAccountManagerForTests(
      std::move(account_manager_ui));  // IN-TEST
}

ScopedAshAccountManagerForTests::~ScopedAshAccountManagerForTests() {
  GetAccountManagerFacadeFactoryLacros()->Reset();
}

account_manager::AccountManagerFacade* GetAccountManagerFacade(
    const std::string& profile_path) {
  // Multi-Login is disabled with Lacros. Always return the same instance.
  return GetAccountManagerFacadeFactoryLacros()->GetAccountManagerFacade();
}

account_manager::AccountManager* MaybeGetAshAccountManagerForTests() {
  return GetAccountManagerFacadeFactoryLacros()
      ->MaybeGetAshAccountManagerForTests();  // IN-TEST
}

account_manager::AccountManagerUI* MaybeGetAshAccountManagerUIForTests() {
  return GetAccountManagerFacadeFactoryLacros()
      ->MaybeGetAshAccountManagerUIForTests();  // IN-TEST
}

crosapi::AccountManagerMojoService*
MaybeGetAshAccountManagerMojoServiceForTests() {
  return GetAccountManagerFacadeFactoryLacros()
      ->MaybeGetAshAccountManagerMojoServiceForTests();  // IN-TEST
}
