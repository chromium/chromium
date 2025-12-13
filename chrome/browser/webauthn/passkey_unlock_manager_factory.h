// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_PASSKEY_UNLOCK_MANAGER_FACTORY_H_
#define CHROME_BROWSER_WEBAUTHN_PASSKEY_UNLOCK_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace webauthn {
class PasskeyUnlockManager;

class PasskeyUnlockManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static PasskeyUnlockManager* GetForProfile(Profile* profile);
  static PasskeyUnlockManagerFactory* GetInstance();

 private:
  friend class base::NoDestructor<PasskeyUnlockManagerFactory>;

  PasskeyUnlockManagerFactory();
  ~PasskeyUnlockManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace webauthn

#endif  // CHROME_BROWSER_WEBAUTHN_PASSKEY_UNLOCK_MANAGER_FACTORY_H_
