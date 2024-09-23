// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_PERMISSION_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_PERMISSION_CONTEXT_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class FederatedIdentityPermissionContext;

// Factory to get or create an instance of FederatedIdentityPermissionContext
// from a Profile.
class FederatedIdentityPermissionContextFactory
    : public ProfileKeyedServiceFactory {
 public:
  static FederatedIdentityPermissionContext* GetForProfile(
      content::BrowserContext* profile);
  static FederatedIdentityPermissionContext* GetForProfileIfExists(
      content::BrowserContext* profile);
  static FederatedIdentityPermissionContextFactory* GetInstance();

 private:
  friend class base::NoDestructor<FederatedIdentityPermissionContextFactory>;

  FederatedIdentityPermissionContextFactory();
  ~FederatedIdentityPermissionContextFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_PERMISSION_CONTEXT_FACTORY_H_
