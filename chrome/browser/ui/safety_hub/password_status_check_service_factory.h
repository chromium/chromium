// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_PASSWORD_STATUS_CHECK_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_PASSWORD_STATUS_CHECK_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class PasswordStatusCheckService;

namespace content {
class BrowserContext;
}

class PasswordStatusCheckServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static PasswordStatusCheckService* GetForProfile(Profile* profile);

  static PasswordStatusCheckServiceFactory* GetInstance();

  // Non-copyable, non-moveable.
  PasswordStatusCheckServiceFactory(const PasswordStatusCheckServiceFactory&) =
      delete;
  PasswordStatusCheckServiceFactory& operator=(
      const PasswordStatusCheckServiceFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<PasswordStatusCheckServiceFactory>;

  PasswordStatusCheckServiceFactory();
  ~PasswordStatusCheckServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_PASSWORD_STATUS_CHECK_SERVICE_FACTORY_H_
