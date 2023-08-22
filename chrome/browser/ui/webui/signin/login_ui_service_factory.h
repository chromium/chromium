// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class LoginUIService;
class Profile;

// Singleton that owns all LoginUIServices and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated LoginUIService.
class LoginUIServiceFactory : public ProfileKeyedServiceFactory {
 public:
  LoginUIServiceFactory(const LoginUIServiceFactory&) = delete;
  LoginUIServiceFactory& operator=(const LoginUIServiceFactory&) = delete;

  // Returns the instance of LoginUIService associated with this profile
  // (creating one if none exists). Returns NULL if this profile cannot have a
  // LoginUIService (for example, if |profile| is incognito).
  static LoginUIService* GetForProfile(Profile* profile);

  // Returns an instance of the LoginUIServiceFactory singleton.
  static LoginUIServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<LoginUIServiceFactory>;

  LoginUIServiceFactory();
  ~LoginUIServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* browser_context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_SERVICE_FACTORY_H_
