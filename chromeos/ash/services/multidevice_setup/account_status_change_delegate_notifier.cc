// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/account_status_change_delegate_notifier.h"

#include "base/logging.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"

namespace ash {

namespace multidevice_setup {

AccountStatusChangeDelegateNotifier::AccountStatusChangeDelegateNotifier() =
    default;

AccountStatusChangeDelegateNotifier::~AccountStatusChangeDelegateNotifier() =
    default;

void AccountStatusChangeDelegateNotifier::SetAccountStatusChangeDelegateRemote(
    mojo::PendingRemote<mojom::AccountStatusChangeDelegate> delegate_remote) {
  if (delegate_remote_.is_bound()) {
    PA_LOG(ERROR) << "AccountStatusChangeDelegateNotifier::"
                  << "SetAccountStatusChangeDelegateRemote(): Tried to set "
                  << "delegate, but one already existed.";
    NOTREACHED_IN_MIGRATION();
  }

  delegate_remote_.Bind(std::move(delegate_remote));
  OnDelegateSet();
}

// No default implementation.
void AccountStatusChangeDelegateNotifier::OnDelegateSet() {}

void AccountStatusChangeDelegateNotifier::FlushForTesting() {
  if (delegate_remote_)
    delegate_remote_.FlushForTesting();
}

}  // namespace multidevice_setup

}  // namespace ash
