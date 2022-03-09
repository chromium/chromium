// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_API_PERMISSION_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_API_PERMISSION_CONTEXT_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class FederatedIdentityApiPermissionContext;

// Factory to get or create an instance of FederatedIdentityApiPermissionContext
// from a Profile.
class FederatedIdentityApiPermissionContextFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static FederatedIdentityApiPermissionContext* GetForProfile(
      content::BrowserContext* profile);
  static FederatedIdentityApiPermissionContextFactory* GetInstance();

 private:
  friend class base::NoDestructor<FederatedIdentityApiPermissionContextFactory>;

  FederatedIdentityApiPermissionContextFactory();
  ~FederatedIdentityApiPermissionContextFactory() override;

  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_API_PERMISSION_CONTEXT_FACTORY_H_
