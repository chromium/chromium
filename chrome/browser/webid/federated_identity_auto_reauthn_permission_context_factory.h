// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION_CONTEXT_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class FederatedIdentityAutoReauthnPermissionContext;

// Factory to get or create an instance of
// FederatedIdentityAutoReauthnPermissionContext from a Profile.
class FederatedIdentityAutoReauthnPermissionContextFactory
    : public ProfileKeyedServiceFactory {
 public:
  static FederatedIdentityAutoReauthnPermissionContext* GetForProfile(
      content::BrowserContext* profile);
  static FederatedIdentityAutoReauthnPermissionContextFactory* GetInstance();

 private:
  friend class base::NoDestructor<
      FederatedIdentityAutoReauthnPermissionContextFactory>;

  FederatedIdentityAutoReauthnPermissionContextFactory();
  ~FederatedIdentityAutoReauthnPermissionContextFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION_CONTEXT_FACTORY_H_
