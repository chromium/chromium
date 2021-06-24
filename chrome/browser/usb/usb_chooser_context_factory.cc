// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_chooser_context_factory.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

UsbChooserContextFactory::UsbChooserContextFactory()
    : BrowserContextKeyedServiceFactory(
          "UsbChooserContext",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

UsbChooserContextFactory::~UsbChooserContextFactory() {}

KeyedService* UsbChooserContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new UsbChooserContext(Profile::FromBrowserContext(context));
}

// static
UsbChooserContextFactory* UsbChooserContextFactory::GetInstance() {
  return base::Singleton<UsbChooserContextFactory>::get();
}

// static
UsbChooserContext* UsbChooserContextFactory::GetForProfile(Profile* profile) {
  return static_cast<UsbChooserContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

UsbChooserContext* UsbChooserContextFactory::GetForProfileIfExists(
    Profile* profile) {
  return static_cast<UsbChooserContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/false));
}

content::BrowserContext* UsbChooserContextFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

void UsbChooserContextFactory::BrowserContextShutdown(
    content::BrowserContext* context) {
  auto* usb_chooser_context =
      GetForProfileIfExists(Profile::FromBrowserContext(context));
  if (usb_chooser_context)
    usb_chooser_context->FlushScheduledSaveSettingsCalls();
}
