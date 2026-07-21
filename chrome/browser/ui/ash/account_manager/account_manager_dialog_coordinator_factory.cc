// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/account_manager/account_manager_dialog_coordinator_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/ui/ash/account_manager/account_manager_dialog_coordinator.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "content/public/browser/browser_context.h"

namespace ash {

// static
AccountManagerDialogCoordinatorFactory*
AccountManagerDialogCoordinatorFactory::GetInstance() {
  static base::NoDestructor<AccountManagerDialogCoordinatorFactory> factory;
  return factory.get();
}

// static
AccountManagerDialogCoordinator*
AccountManagerDialogCoordinatorFactory::GetForProfile(Profile* profile) {
  return static_cast<AccountManagerDialogCoordinator*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

AccountManagerDialogCoordinatorFactory::AccountManagerDialogCoordinatorFactory()
    : ProfileKeyedServiceFactory(
          "AccountManagerDialogCoordinator",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(AccountReconcilorFactory::GetInstance());
}

AccountManagerDialogCoordinatorFactory::
    ~AccountManagerDialogCoordinatorFactory() = default;

std::unique_ptr<KeyedService>
AccountManagerDialogCoordinatorFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile);

  // ChromeOS account dialogs do not produce a natural app-foreground or
  // lifecycle event. Reconcile as soon as a dialog flow finishes so browser
  // cookies are immediately brought back in sync.
  return std::make_unique<AccountManagerDialogCoordinator>(
      reconcilor->CreateForceReconcileCallback());
}

}  // namespace ash
