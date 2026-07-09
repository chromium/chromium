// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/account_manager/scoped_fake_account_manager_dialog.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/account_manager/account_manager_dialog_coordinator.h"
#include "chrome/browser/ui/ash/account_manager/account_manager_dialog_coordinator_factory.h"

namespace ash::test {

ScopedFakeAccountManagerDialog::ScopedFakeAccountManagerDialog(
    Profile* profile) {
  CHECK(profile);
  AccountManagerDialogCoordinator* coordinator =
      AccountManagerDialogCoordinatorFactory::GetForProfile(profile);
  CHECK(coordinator);
  reset_dialog_callbacks_ = coordinator->InstallDialogCallbacksForTesting(
      base::BindRepeating(&FakeAccountManagerDialog::ShowAddAccountDialog,
                          base::Unretained(&fake_account_manager_dialog_)),
      base::BindRepeating(&FakeAccountManagerDialog::ShowReauthAccountDialog,
                          base::Unretained(&fake_account_manager_dialog_)),
      base::BindRepeating(&FakeAccountManagerDialog::IsDialogShown,
                          base::Unretained(&fake_account_manager_dialog_)));
}

ScopedFakeAccountManagerDialog::~ScopedFakeAccountManagerDialog() = default;

}  // namespace ash::test
