// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_ENCLAVE_MANAGER_FACTORY_H_
#define CHROME_BROWSER_WEBAUTHN_ENCLAVE_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class EnclaveManager;
class Profile;

class EnclaveManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static EnclaveManager* GetForProfile(Profile* profile);
  static EnclaveManagerFactory* GetInstance();

 private:
  friend base::NoDestructor<EnclaveManagerFactory>;

  EnclaveManagerFactory();
  ~EnclaveManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_WEBAUTHN_ENCLAVE_MANAGER_FACTORY_H_
