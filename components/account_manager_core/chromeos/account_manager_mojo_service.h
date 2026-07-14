// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_MANAGER_CORE_CHROMEOS_ACCOUNT_MANAGER_MOJO_SERVICE_H_
#define COMPONENTS_ACCOUNT_MANAGER_CORE_CHROMEOS_ACCOUNT_MANAGER_MOJO_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// Implements the |crosapi::mojom::AccountManager| interface in ash-chrome.
// It enables lacros-chrome to interact with accounts stored in the Chrome OS
// Account Manager.
//
// TODO(b/365741912, b/365902693): Delete this service and the
// crosapi::mojom::AccountManager interface after the remaining facade users are
// migrated to direct in-process Account Manager APIs.
class COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE) AccountManagerMojoService
    : public mojom::AccountManager {
 public:
  explicit AccountManagerMojoService(
      account_manager::AccountManager* account_manager);
  AccountManagerMojoService(const AccountManagerMojoService&) = delete;
  AccountManagerMojoService& operator=(const AccountManagerMojoService&) =
      delete;
  ~AccountManagerMojoService() override;

  void BindReceiver(mojo::PendingReceiver<mojom::AccountManager> receiver);

 private:
  friend class AccountManagerMojoServiceTest;

  const raw_ptr<account_manager::AccountManager> account_manager_;

  // Don't add new members below this. `receivers_` should be destroyed as soon
  // as `this` is getting destroyed so that we don't deal with message handling
  // on a partially destroyed object.
  mojo::ReceiverSet<mojom::AccountManager> receivers_;

  base::WeakPtrFactory<AccountManagerMojoService> weak_ptr_factory_{this};
};

}  // namespace crosapi

#endif  // COMPONENTS_ACCOUNT_MANAGER_CORE_CHROMEOS_ACCOUNT_MANAGER_MOJO_SERVICE_H_
