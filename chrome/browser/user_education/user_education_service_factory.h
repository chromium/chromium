// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USER_EDUCATION_USER_EDUCATION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_USER_EDUCATION_USER_EDUCATION_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

class UserEducationService;
namespace internal {
class InteractiveFeaturePromoTestPrivate;
}

class UserEducationServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Disallow copy/assign.
  UserEducationServiceFactory(const UserEducationServiceFactory&) = delete;
  void operator=(const UserEducationServiceFactory&) = delete;

  static UserEducationServiceFactory* GetInstance();
  static UserEducationService* GetForBrowserContext(
      content::BrowserContext* context);

  static bool ProfileAllowsUserEducation(Profile* profile);

  // Prevents polling of the idle state in cases where the extra observer would
  // interfere with the test.
  void disable_idle_polling_for_testing() { disable_idle_polling_ = true; }

 private:
  friend base::NoDestructor<UserEducationServiceFactory>;
  friend internal::InteractiveFeaturePromoTestPrivate;

  // Used internally and by some test code.
  static std::unique_ptr<UserEducationService>
  BuildServiceInstanceForBrowserContextImpl(content::BrowserContext* context,
                                            bool disable_idle_polling);

  // BrowserContextKeyedServiceFactory overrides.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  UserEducationServiceFactory();
  ~UserEducationServiceFactory() override;

  bool disable_idle_polling_ = false;
};

#endif  // CHROME_BROWSER_USER_EDUCATION_USER_EDUCATION_SERVICE_FACTORY_H_
