// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_ACCOUNT_STATUS_CHANGE_DELEGATE_NOTIFIER_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_ACCOUNT_STATUS_CHANGE_DELEGATE_NOTIFIER_H_

#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

namespace multidevice_setup {

// Notifies the delegate of MultiDeviceSetup for each of the following changes:
// (1) a potential host is found for someone who has not gone through the setup
//     flow before,
// (2) the host has switched for someone who has, or
// (3) a new Chromebook has been added to an account for someone who has.
class AccountStatusChangeDelegateNotifier {
 public:
  AccountStatusChangeDelegateNotifier(
      const AccountStatusChangeDelegateNotifier&) = delete;
  AccountStatusChangeDelegateNotifier& operator=(
      const AccountStatusChangeDelegateNotifier&) = delete;

  virtual ~AccountStatusChangeDelegateNotifier();

  void SetAccountStatusChangeDelegateRemote(
      mojo::PendingRemote<mojom::AccountStatusChangeDelegate> delegate_remote);

 protected:
  AccountStatusChangeDelegateNotifier();

  // Derived classes should override this function to be alerted when
  // SetAccountStatusChangeDelegateRemote() is called.
  virtual void OnDelegateSet();

  mojom::AccountStatusChangeDelegate* delegate() {
    return delegate_remote_.is_bound() ? delegate_remote_.get() : nullptr;
  }

 private:
  friend class MultiDeviceSetupImpl;
  friend class MultiDeviceSetupAccountStatusChangeDelegateNotifierTest;
  friend class MultiDeviceSetupImplTest;
  friend class MultiDeviceSetupWifiSyncNotificationControllerTest;
  friend class WifiSyncNotificationController;

  void FlushForTesting();

  mojo::Remote<mojom::AccountStatusChangeDelegate> delegate_remote_;
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_ACCOUNT_STATUS_CHANGE_DELEGATE_NOTIFIER_H_
