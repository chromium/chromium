// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"

#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

bool ArcAppListPrefsFactory::is_sync_test_ = false;

// static
ArcAppListPrefs* ArcAppListPrefsFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ArcAppListPrefs*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ArcAppListPrefsFactory* ArcAppListPrefsFactory::GetInstance() {
  return base::Singleton<ArcAppListPrefsFactory>::get();
}

// static
void ArcAppListPrefsFactory::SetFactoryForSyncTest() {
  is_sync_test_ = true;
}

// static
bool ArcAppListPrefsFactory::IsFactorySetForSyncTest() {
  return is_sync_test_;
}

void ArcAppListPrefsFactory::RecreateServiceInstanceForTesting(
    content::BrowserContext* context) {
  Disassociate(context);
  BuildServiceInstanceFor(context);
}

ArcAppListPrefsFactory::ArcAppListPrefsFactory()
    : BrowserContextKeyedServiceFactory(
          "ArcAppListPrefs",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
}

ArcAppListPrefsFactory::~ArcAppListPrefsFactory() {
}

KeyedService* ArcAppListPrefsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);

  // Profiles are always treated as allowed to ARC in sync integration test.
  if (is_sync_test_) {
    sync_test_app_connection_holders_[context] = std::make_unique<
        arc::ConnectionHolder<arc::mojom::AppInstance, arc::mojom::AppHost>>();
    return ArcAppListPrefs::Create(
        profile, sync_test_app_connection_holders_[context].get());
  }

  if (!arc::IsArcAllowedForProfile(profile))
    return nullptr;

  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager)
    return nullptr;  // ARC is not supported

  return ArcAppListPrefs::Create(profile);
}

content::BrowserContext* ArcAppListPrefsFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // This matches the logic in ExtensionSyncServiceFactory, which uses the
  // orginal browser context.
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}
