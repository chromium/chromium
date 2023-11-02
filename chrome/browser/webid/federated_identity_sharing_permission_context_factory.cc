// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_sharing_permission_context_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webid/federated_identity_sharing_permission_context.h"

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
    : ProfileKeyedServiceFactory(
          "FederatedIdentitySharingPermissionContext",
          ProfileSelections::BuildForRegularAndIncognito()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

FederatedIdentitySharingPermissionContextFactory::
    ~FederatedIdentitySharingPermissionContextFactory() = default;

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
