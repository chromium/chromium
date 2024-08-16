// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USER_ANNOTATIONS_USER_ANNOTATIONS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_USER_ANNOTATIONS_USER_ANNOTATIONS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace user_annotations {
class UserAnnotationsService;
}  // namespace user_annotations

class Profile;

// LazyInstance that owns all UserAnnotationsService(s) and associates
// them with Profiles.
class UserAnnotationsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Gets the UserAnnotationsService for the profile.
  //
  // Returns null if the features that allow for this to provide useful
  // information are disabled.
  static user_annotations::UserAnnotationsService* GetForProfile(
      Profile* profile);

  // Gets the LazyInstance that owns all UserAnnotationsService(s).
  static UserAnnotationsServiceFactory* GetInstance();

  UserAnnotationsServiceFactory(const UserAnnotationsServiceFactory&) = delete;
  UserAnnotationsServiceFactory& operator=(
      const UserAnnotationsServiceFactory&) = delete;

 private:
  friend base::NoDestructor<UserAnnotationsServiceFactory>;

  UserAnnotationsServiceFactory();
  ~UserAnnotationsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_USER_ANNOTATIONS_USER_ANNOTATIONS_SERVICE_FACTORY_H_
