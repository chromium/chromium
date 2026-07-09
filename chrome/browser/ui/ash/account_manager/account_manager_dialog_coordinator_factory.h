// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ACCOUNT_MANAGER_ACCOUNT_MANAGER_DIALOG_COORDINATOR_FACTORY_H_
#define CHROME_BROWSER_UI_ASH_ACCOUNT_MANAGER_ACCOUNT_MANAGER_DIALOG_COORDINATOR_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {

class AccountManagerDialogCoordinator;

class AccountManagerDialogCoordinatorFactory
    : public ProfileKeyedServiceFactory {
 public:
  AccountManagerDialogCoordinatorFactory(
      const AccountManagerDialogCoordinatorFactory&) = delete;
  AccountManagerDialogCoordinatorFactory& operator=(
      const AccountManagerDialogCoordinatorFactory&) = delete;

  static AccountManagerDialogCoordinatorFactory* GetInstance();
  static AccountManagerDialogCoordinator* GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<AccountManagerDialogCoordinatorFactory>;

  AccountManagerDialogCoordinatorFactory();
  ~AccountManagerDialogCoordinatorFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_ACCOUNT_MANAGER_ACCOUNT_MANAGER_DIALOG_COORDINATOR_FACTORY_H_
