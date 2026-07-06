// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_CMTG_DEVICE_KEY_PROVIDER_FACTORY_H_
#define CHROME_BROWSER_WEBAUTHN_CMTG_DEVICE_KEY_PROVIDER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace webauthn {
class CmtgDeviceKeyProvider;
}

class Profile;

class CmtgDeviceKeyProviderFactory : public ProfileKeyedServiceFactory {
 public:
  static CmtgDeviceKeyProviderFactory* GetInstance();
  static webauthn::CmtgDeviceKeyProvider* GetForProfile(Profile* profile);

 private:
  friend class base::NoDestructor<CmtgDeviceKeyProviderFactory>;

  CmtgDeviceKeyProviderFactory();
  ~CmtgDeviceKeyProviderFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_WEBAUTHN_CMTG_DEVICE_KEY_PROVIDER_FACTORY_H_
