// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_AUTO_SIGNIN_PERMISSION_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_AUTO_SIGNIN_PERMISSION_CONTEXT_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class FederatedIdentityAutoSigninPermissionContext;

// Factory to get or create an instance of
// FederatedIdentityAutoSigninPermissionContext from a Profile.
class FederatedIdentityAutoSigninPermissionContextFactory
    : public ProfileKeyedServiceFactory {
 public:
  static FederatedIdentityAutoSigninPermissionContext* GetForProfile(
      content::BrowserContext* profile);
  static FederatedIdentityAutoSigninPermissionContextFactory* GetInstance();

 private:
  friend class base::NoDestructor<
      FederatedIdentityAutoSigninPermissionContextFactory>;

  FederatedIdentityAutoSigninPermissionContextFactory();
  ~FederatedIdentityAutoSigninPermissionContextFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_AUTO_SIGNIN_PERMISSION_CONTEXT_FACTORY_H_
