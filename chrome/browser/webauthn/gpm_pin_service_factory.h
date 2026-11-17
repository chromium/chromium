// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_GPM_PIN_SERVICE_FACTORY_H_
#define CHROME_BROWSER_WEBAUTHN_GPM_PIN_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace webauthn {

class GpmPinService;

// Only registered when `device::kWebAuthnLocalGpmPasskeys` is enabled, so
// `GetInstance()` and factories that depend on this one must check the flag.
class GpmPinServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns null if `device::kWebAuthnLocalGpmPasskeys` is disabled or
  // `profile` is not a regular profile.
  static GpmPinService* GetForProfile(Profile* profile);
  static GpmPinServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<GpmPinServiceFactory>;

  GpmPinServiceFactory();
  ~GpmPinServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace webauthn

#endif  // CHROME_BROWSER_WEBAUTHN_GPM_PIN_SERVICE_FACTORY_H_
