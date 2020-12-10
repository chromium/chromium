// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_FACADE_H_
#define COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_FACADE_H_

#include "base/component_export.h"

namespace account_manager {

// An interface to talk to |AccountManager|.
// Implementations of this interface hide the in-process / out-of-process nature
// of this communication.
// Instances of this class are singletons, and are independent of a |Profile|.
// Use |GetAccountManagerFacade()| to get an instance of this class.
class COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE) AccountManagerFacade {
 public:
  // The source UI surface used for launching the account addition /
  // re-authentication dialog. This should be as specific as possible.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // Note: Please update |AccountManagerAccountAdditionSource| in enums.xml
  // after adding new values.
  enum class AccountAdditionSource : int {
    // Settings > Add account button.
    kSettingsAddAccountButton = 0,
    // Settings > Sign in again button.
    kSettingsReauthAccountButton = 1,
    // Launched from an ARC application.
    kArc = 2,
    // Launched automatically from Chrome content area. As of now, this is
    // possible only when an account requires re-authentication.
    kContentArea = 3,
    // Print Preview dialog.
    kPrintPreviewDialog = 4,
    // Account Manager migration welcome screen.
    kAccountManagerMigrationWelcomeScreen = 5,
    // Onboarding.
    kOnboarding = 6,

    kMaxValue = kOnboarding
  };

  AccountManagerFacade();
  AccountManagerFacade(const AccountManagerFacade&) = delete;
  AccountManagerFacade& operator=(const AccountManagerFacade&) = delete;
  virtual ~AccountManagerFacade() = 0;

  // Returns |true| if |AccountManager| is connected and has been fully
  // initialized.
  // Note: For out-of-process implementations, it returns |false| if the IPC
  // pipe to |AccountManager| is disconnected.
  virtual bool IsInitialized() = 0;
};

}  // namespace account_manager

#endif  // COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_FACADE_H_
