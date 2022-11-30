// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_ACTIVE_SESSION_PERMISSION_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_ACTIVE_SESSION_PERMISSION_CONTEXT_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class FederatedIdentityActiveSessionPermissionContext;

// Factory to get or create an instance of
// FederatedIdentityActiveSessionPermissionContext from a Profile.
class FederatedIdentityActiveSessionPermissionContextFactory
    : public ProfileKeyedServiceFactory {
 public:
  static FederatedIdentityActiveSessionPermissionContext* GetForProfile(
      content::BrowserContext* profile);
  static FederatedIdentityActiveSessionPermissionContextFactory* GetInstance();

 private:
  friend class base::NoDestructor<
      FederatedIdentityActiveSessionPermissionContextFactory>;

  FederatedIdentityActiveSessionPermissionContextFactory();
  ~FederatedIdentityActiveSessionPermissionContextFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  void BrowserContextShutdown(content::BrowserContext* context) override;
};

#endif  // CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_ACTIVE_SESSION_PERMISSION_CONTEXT_FACTORY_H_
