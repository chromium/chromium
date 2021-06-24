// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_sharing_permission_context_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webid/federated_identity_sharing_permission_context.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
FederatedIdentitySharingPermissionContext*
FederatedIdentitySharingPermissionContextFactory::GetForProfile(
    content::BrowserContext* profile) {
  return static_cast<FederatedIdentitySharingPermissionContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
FederatedIdentitySharingPermissionContextFactory*
FederatedIdentitySharingPermissionContextFactory::GetInstance() {
  static base::NoDestructor<FederatedIdentitySharingPermissionContextFactory>
      instance;
  return instance.get();
}

FederatedIdentitySharingPermissionContextFactory::
    FederatedIdentitySharingPermissionContextFactory()
    : BrowserContextKeyedServiceFactory(
          "FederatedIdentitySharingPermissionContext",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

FederatedIdentitySharingPermissionContextFactory::
    ~FederatedIdentitySharingPermissionContextFactory() = default;

content::BrowserContext*
FederatedIdentitySharingPermissionContextFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

KeyedService*
FederatedIdentitySharingPermissionContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new FederatedIdentitySharingPermissionContext(profile);
}

void FederatedIdentitySharingPermissionContextFactory::BrowserContextShutdown(
    content::BrowserContext* context) {
  GetForProfile(Profile::FromBrowserContext(context))
      ->FlushScheduledSaveSettingsCalls();
}
