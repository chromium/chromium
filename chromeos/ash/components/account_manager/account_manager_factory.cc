// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/account_manager/account_manager_factory.h"

#include <string>
#include <utility>

#include "base/callback_list.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/callback.h"
#include "components/account_manager_core/account_manager_facade_impl.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/account_manager_core/chromeos/account_manager_mojo_service.h"

namespace ash {
namespace {
static AccountManagerFactory* g_instance = nullptr;
}

AccountManagerFactory::AccountManagerFactory() {
  CHECK(!g_instance);
  g_instance = this;
}
AccountManagerFactory::~AccountManagerFactory() {
  on_destruction_callbacks_.Notify();

  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
AccountManagerFactory* AccountManagerFactory::Get() {
  return g_instance;
}

account_manager::AccountManager* AccountManagerFactory::GetAccountManager(
    const std::string& profile_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return GetAccountManagerHolder(profile_path).account_manager.get();
}

crosapi::AccountManagerMojoService*
AccountManagerFactory::GetAccountManagerMojoService(
    const std::string& profile_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return GetAccountManagerHolder(profile_path)
      .account_manager_mojo_service.get();
}

account_manager::AccountManagerFacade*
AccountManagerFactory::GetAccountManagerFacade(
    const std::string& profile_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return GetAccountManagerHolder(profile_path).account_manager_facade.get();
}

base::CallbackListSubscription AccountManagerFactory::AddOnDestructionCallback(
    base::OnceClosure callback) {
  return on_destruction_callbacks_.Add(std::move(callback));
}

AccountManagerFactory::AccountManagerHolder::AccountManagerHolder(
    std::unique_ptr<account_manager::AccountManager> account_manager,
    std::unique_ptr<crosapi::AccountManagerMojoService>
        account_manager_mojo_service,
    std::unique_ptr<account_manager::AccountManagerFacade>
        account_manager_facade)
    : account_manager(std::move(account_manager)),
      account_manager_mojo_service(std::move(account_manager_mojo_service)),
      account_manager_facade(std::move(account_manager_facade)) {}

AccountManagerFactory::AccountManagerHolder::~AccountManagerHolder() = default;

const AccountManagerFactory::AccountManagerHolder&
AccountManagerFactory::GetAccountManagerHolder(
    const std::string& profile_path) {
  auto it = account_managers_.find(profile_path);
  if (it == account_managers_.end()) {
    auto account_manager = std::make_unique<account_manager::AccountManager>();
    auto account_manager_mojo_service =
        std::make_unique<crosapi::AccountManagerMojoService>(
            account_manager.get());

    mojo::Remote<crosapi::mojom::AccountManager> remote;
    account_manager_mojo_service->BindReceiver(
        remote.BindNewPipeAndPassReceiver());

    // This is set to a sentinel value which will pass all minimum version
    // checks.
    // Calls within Ash are in the same process and don't need to check version
    // compatibility with itself.
    constexpr uint32_t remote_version = std::numeric_limits<uint32_t>::max();
    // TODO(crbug.com/40800999): to avoid incorrect usage, pass a nullptr
    // `AccountManager` when this is not running in a test.
    base::WeakPtr<account_manager::AccountManager> account_manager_for_tests =
        account_manager->GetWeakPtr();
    auto account_manager_facade =
        std::make_unique<account_manager::AccountManagerFacadeImpl>(
            std::move(remote), remote_version, account_manager_for_tests);

    it = account_managers_
             .emplace(
                 std::piecewise_construct, std::forward_as_tuple(profile_path),
                 std::forward_as_tuple(std::move(account_manager),
                                       std::move(account_manager_mojo_service),
                                       std::move(account_manager_facade)))
             .first;
  }
  return it->second;
}

}  // namespace ash
