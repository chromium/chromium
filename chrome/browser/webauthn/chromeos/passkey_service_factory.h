// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_CHROMEOS_PASSKEY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_WEBAUTHN_CHROMEOS_PASSKEY_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace chromeos {

class PasskeyService;

class PasskeyServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static PasskeyServiceFactory* GetInstance();
  static PasskeyService* GetForProfile(Profile* profile);

 private:
  friend class base::NoDestructor<PasskeyServiceFactory>;

  PasskeyServiceFactory();
  ~PasskeyServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_WEBAUTHN_CHROMEOS_PASSKEY_SERVICE_FACTORY_H_
