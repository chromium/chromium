// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_FACADE_IMPL_H_
#define COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_FACADE_IMPL_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

// ChromeOS-specific implementation of |AccountManagerFacade| that talks to
// |chromeos::AccountManager| over Mojo. Used by both Lacros and Ash.
class COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE) AccountManagerFacadeImpl
    : public account_manager::AccountManagerFacade,
      public crosapi::mojom::AccountManagerObserver {
 public:
  AccountManagerFacadeImpl(
      mojo::Remote<crosapi::mojom::AccountManager> account_manager_remote,
      base::OnceClosure init_finished = base::DoNothing());
  AccountManagerFacadeImpl(const AccountManagerFacadeImpl&) = delete;
  AccountManagerFacadeImpl& operator=(const AccountManagerFacadeImpl&) = delete;
  ~AccountManagerFacadeImpl() override;

  // AccountManagerFacade overrides:
  bool IsInitialized() override;
  void ShowAddAccountDialog(
      const AccountAdditionSource& source,
      base::OnceCallback<void(const AccountAdditionResult& result)> callback)
      override;
  void ShowReauthAccountDialog(const AccountAdditionSource& source,
                               const std::string& email) override;

  // crosapi::mojom::AccountManagerObserver overrides:
  void OnTokenUpserted(crosapi::mojom::AccountPtr account) override;
  void OnAccountRemoved(crosapi::mojom::AccountPtr account) override;

 private:
  void OnVersionCheck(uint32_t version);
  void OnReceiverReceived(
      mojo::PendingReceiver<AccountManagerObserver> receiver);
  void OnInitialized(bool is_initialized);

  mojo::Remote<crosapi::mojom::AccountManager> account_manager_remote_;
  base::OnceClosure init_finished_;
  bool is_initialized_ = false;
  std::unique_ptr<mojo::Receiver<crosapi::mojom::AccountManagerObserver>>
      receiver_;

  base::WeakPtrFactory<AccountManagerFacadeImpl> weak_factory_{this};
};

#endif  // COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_FACADE_IMPL_H_
