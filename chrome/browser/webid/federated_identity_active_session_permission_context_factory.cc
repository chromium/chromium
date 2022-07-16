// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_active_session_permission_context_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webid/federated_identity_active_session_permission_context.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
FederatedIdentityActiveSessionPermissionContext*
FederatedIdentityActiveSessionPermissionContextFactory::GetForProfile(
    content::BrowserContext* profile) {
  return static_cast<FederatedIdentityActiveSessionPermissionContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
FederatedIdentityActiveSessionPermissionContextFactory*
FederatedIdentityActiveSessionPermissionContextFactory::GetInstance() {
  static base::NoDestructor<
      FederatedIdentityActiveSessionPermissionContextFactory>
      instance;
  return instance.get();
}

FederatedIdentityActiveSessionPermissionContextFactory::
    FederatedIdentityActiveSessionPermissionContextFactory()
    : BrowserContextKeyedServiceFactory(
          "FederatedIdentityActiveSessionPermissionContext",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

FederatedIdentityActiveSessionPermissionContextFactory::
    ~FederatedIdentityActiveSessionPermissionContextFactory() = default;

content::BrowserContext*
FederatedIdentityActiveSessionPermissionContextFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

KeyedService*
FederatedIdentityActiveSessionPermissionContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new FederatedIdentityActiveSessionPermissionContext(profile);
}

void FederatedIdentityActiveSessionPermissionContextFactory::
    BrowserContextShutdown(content::BrowserContext* context) {
  GetForProfile(Profile::FromBrowserContext(context))
      ->FlushScheduledSaveSettingsCalls();
}
