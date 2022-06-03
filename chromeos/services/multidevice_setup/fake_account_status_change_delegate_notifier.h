// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_ACCOUNT_STATUS_CHANGE_DELEGATE_NOTIFIER_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_ACCOUNT_STATUS_CHANGE_DELEGATE_NOTIFIER_H_

#include "chromeos/services/multidevice_setup/account_status_change_delegate_notifier.h"

namespace chromeos {

namespace multidevice_setup {

// Test AccountStatusChangeDelegateNotifier implementation.
class FakeAccountStatusChangeDelegateNotifier
    : public AccountStatusChangeDelegateNotifier {
 public:
  FakeAccountStatusChangeDelegateNotifier() = default;

  FakeAccountStatusChangeDelegateNotifier(
      const FakeAccountStatusChangeDelegateNotifier&) = delete;
  FakeAccountStatusChangeDelegateNotifier& operator=(
      const FakeAccountStatusChangeDelegateNotifier&) = delete;

  ~FakeAccountStatusChangeDelegateNotifier() override = default;

  using AccountStatusChangeDelegateNotifier::delegate;
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_ACCOUNT_STATUS_CHANGE_DELEGATE_NOTIFIER_H_
