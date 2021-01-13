// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/account_manager_facade_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"

namespace {

// Interface versions in //chromeos/crosapi/mojom/account_manager.mojom:
// MinVersion of crosapi::mojom::AccountManager::AddObserver
constexpr uint32_t kMinVersionWithObserver = 1;

}  // namespace

AccountManagerFacadeImpl::AccountManagerFacadeImpl(
    mojo::Remote<crosapi::mojom::AccountManager> account_manager_remote,
    base::OnceClosure init_finished)
    : account_manager_remote_(std::move(account_manager_remote)),
      init_finished_(std::move(init_finished)) {
  DCHECK(init_finished_);
  if (!account_manager_remote_) {
    std::move(init_finished_).Run();
    return;
  }

  account_manager_remote_.QueryVersion(base::BindOnce(
      &AccountManagerFacadeImpl::OnVersionCheck, weak_factory_.GetWeakPtr()));
}

AccountManagerFacadeImpl::~AccountManagerFacadeImpl() = default;

bool AccountManagerFacadeImpl::IsInitialized() {
  return is_initialized_;
}

void AccountManagerFacadeImpl::ShowAddAccountDialog(
    const AccountAdditionSource& source,
    base::OnceCallback<void(const AccountAdditionResult& result)> callback) {
  // TODO(crbug.com/1140469): implement this.
}

void AccountManagerFacadeImpl::ShowReauthAccountDialog(
    const AccountAdditionSource& source,
    const std::string& email) {
  // TODO(crbug.com/1140469): implement this.
}

void AccountManagerFacadeImpl::OnVersionCheck(uint32_t version) {
  if (version < kMinVersionWithObserver) {
    std::move(init_finished_).Run();
    return;
  }

  account_manager_remote_->AddObserver(
      base::BindOnce(&AccountManagerFacadeImpl::OnReceiverReceived,
                     weak_factory_.GetWeakPtr()));
}

void AccountManagerFacadeImpl::OnReceiverReceived(
    mojo::PendingReceiver<AccountManagerObserver> receiver) {
  receiver_ =
      std::make_unique<mojo::Receiver<crosapi::mojom::AccountManagerObserver>>(
          this, std::move(receiver));
  // At this point (|receiver_| exists), we are subscribed to Account Manager.

  account_manager_remote_->IsInitialized(base::BindOnce(
      &AccountManagerFacadeImpl::OnInitialized, weak_factory_.GetWeakPtr()));
}
void AccountManagerFacadeImpl::OnInitialized(bool is_initialized) {
  if (is_initialized)
    is_initialized_ = true;
  // else: We will receive a notification in |OnTokenUpserted|.
  std::move(init_finished_).Run();
}

void AccountManagerFacadeImpl::OnTokenUpserted(
    crosapi::mojom::AccountPtr account) {
  is_initialized_ = true;
}

void AccountManagerFacadeImpl::OnAccountRemoved(
    crosapi::mojom::AccountPtr account) {}
