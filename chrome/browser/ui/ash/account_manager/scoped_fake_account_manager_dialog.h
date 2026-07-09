// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ACCOUNT_MANAGER_SCOPED_FAKE_ACCOUNT_MANAGER_DIALOG_H_
#define CHROME_BROWSER_UI_ASH_ACCOUNT_MANAGER_SCOPED_FAKE_ACCOUNT_MANAGER_DIALOG_H_

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/ash/account_manager/fake_account_manager_dialog.h"

class Profile;

namespace ash::test {

// Installs a FakeAccountManagerDialog on the profile's dialog coordinator for
// as long as this object is alive. Destruction resets the coordinator's test
// callbacks to null (not to whatever was installed before), so don't nest two
// on the same profile.
class ScopedFakeAccountManagerDialog {
 public:
  explicit ScopedFakeAccountManagerDialog(Profile* profile);
  ScopedFakeAccountManagerDialog(const ScopedFakeAccountManagerDialog&) =
      delete;
  ScopedFakeAccountManagerDialog& operator=(
      const ScopedFakeAccountManagerDialog&) = delete;
  ~ScopedFakeAccountManagerDialog();

  FakeAccountManagerDialog* operator->() {
    return &fake_account_manager_dialog_;
  }

 private:
  FakeAccountManagerDialog fake_account_manager_dialog_;
  base::ScopedClosureRunner reset_dialog_callbacks_;
};

}  // namespace ash::test

#endif  // CHROME_BROWSER_UI_ASH_ACCOUNT_MANAGER_SCOPED_FAKE_ACCOUNT_MANAGER_DIALOG_H_
