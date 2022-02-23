// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_usb_host_permission_manager_factory.h"

#include "ash/components/arc/usb/usb_host_bridge.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_usb_host_permission_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace arc {

// static
ArcUsbHostPermissionManager*
ArcUsbHostPermissionManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ArcUsbHostPermissionManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ArcUsbHostPermissionManagerFactory*
ArcUsbHostPermissionManagerFactory::GetInstance() {
  return base::Singleton<ArcUsbHostPermissionManagerFactory>::get();
}

ArcUsbHostPermissionManagerFactory::ArcUsbHostPermissionManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "ArcUsbHostPermissionManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ArcAppListPrefsFactory::GetInstance());
  DependsOn(ArcUsbHostBridge::GetFactory());
}

ArcUsbHostPermissionManagerFactory::~ArcUsbHostPermissionManagerFactory() {}

KeyedService* ArcUsbHostPermissionManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return ArcUsbHostPermissionManager::Create(context);
}

content::BrowserContext*
ArcUsbHostPermissionManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // This matches the logic in ExtensionSyncServiceFactory, which uses the
  // orginal browser context.
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace arc
