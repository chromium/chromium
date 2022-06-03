// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_request_permission_context_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webid/federated_identity_request_permission_context.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
FederatedIdentityRequestPermissionContext*
FederatedIdentityRequestPermissionContextFactory::GetForProfile(
    content::BrowserContext* profile) {
  return static_cast<FederatedIdentityRequestPermissionContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
FederatedIdentityRequestPermissionContextFactory*
FederatedIdentityRequestPermissionContextFactory::GetInstance() {
  static base::NoDestructor<FederatedIdentityRequestPermissionContextFactory>
      instance;
  return instance.get();
}

FederatedIdentityRequestPermissionContextFactory::
    FederatedIdentityRequestPermissionContextFactory()
    : BrowserContextKeyedServiceFactory(
          "FederatedIdentityRequestPermissionContext",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

FederatedIdentityRequestPermissionContextFactory::
    ~FederatedIdentityRequestPermissionContextFactory() = default;

content::BrowserContext*
FederatedIdentityRequestPermissionContextFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

KeyedService*
FederatedIdentityRequestPermissionContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new FederatedIdentityRequestPermissionContext(profile);
}

void FederatedIdentityRequestPermissionContextFactory::BrowserContextShutdown(
    content::BrowserContext* context) {
  GetForProfile(Profile::FromBrowserContext(context))
      ->FlushScheduledSaveSettingsCalls();
}
