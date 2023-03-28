// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_PASSKEY_MODEL_FACTORY_H_
#define CHROME_BROWSER_WEBAUTHN_PASSKEY_MODEL_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/webauthn/core/browser/passkey_model.h"

namespace content {
class BrowserContext;
}
// class PasskeyModel;

class PasskeyModelFactory : public ProfileKeyedServiceFactory {
 public:
  static PasskeyModelFactory* GetInstance();
  static PasskeyModel* GetForProfile(Profile* profile);

 private:
  friend class base::NoDestructor<PasskeyModelFactory>;

  PasskeyModelFactory();
  ~PasskeyModelFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_WEBAUTHN_PASSKEY_MODEL_FACTORY_H_
