// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_active_session_permission_context_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webid/federated_identity_active_session_permission_context.h"

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
    : ProfileKeyedServiceFactory(
          "FederatedIdentityActiveSessionPermissionContext",
          ProfileSelections::BuildForRegularAndIncognito()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

FederatedIdentityActiveSessionPermissionContextFactory::
    ~FederatedIdentityActiveSessionPermissionContextFactory() = default;

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
