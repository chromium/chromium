// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_API_PERMISSION_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_API_PERMISSION_CONTEXT_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class FederatedIdentityApiPermissionContext;

// Factory to get or create an instance of FederatedIdentityApiPermissionContext
// from a Profile.
class FederatedIdentityApiPermissionContextFactory
    : public ProfileKeyedServiceFactory {
 public:
  static FederatedIdentityApiPermissionContext* GetForProfile(
      content::BrowserContext* profile);
  static FederatedIdentityApiPermissionContextFactory* GetInstance();

 private:
  friend class base::NoDestructor<FederatedIdentityApiPermissionContextFactory>;

  FederatedIdentityApiPermissionContextFactory();
  ~FederatedIdentityApiPermissionContextFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_API_PERMISSION_CONTEXT_FACTORY_H_
