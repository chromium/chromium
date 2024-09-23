// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_HATS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_HATS_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class SafetyHubHatsService;

namespace base {
template <typename T>
class NoDestructor;
}

namespace content {
class BrowserContext;
}

class SafetyHubHatsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static SafetyHubHatsServiceFactory* GetInstance();

  static SafetyHubHatsService* GetForProfile(Profile* profile);

  // Non-copyable, non-moveable.
  SafetyHubHatsServiceFactory(const SafetyHubHatsServiceFactory&) = delete;
  SafetyHubHatsServiceFactory& operator=(const SafetyHubHatsServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<SafetyHubHatsServiceFactory>;

  SafetyHubHatsServiceFactory();
  ~SafetyHubHatsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_HATS_SERVICE_FACTORY_H_
