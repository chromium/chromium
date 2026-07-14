// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/chromeos/account_manager_mojo_service.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace crosapi {

AccountManagerMojoService::AccountManagerMojoService(
    account_manager::AccountManager* account_manager)
    : account_manager_(account_manager) {
  CHECK(account_manager_);
}

AccountManagerMojoService::~AccountManagerMojoService() = default;

void AccountManagerMojoService::BindReceiver(
    mojo::PendingReceiver<mojom::AccountManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

}  // namespace crosapi
