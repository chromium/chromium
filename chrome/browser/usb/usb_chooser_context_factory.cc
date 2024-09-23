// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_chooser_context_factory.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/usb/usb_chooser_context.h"

UsbChooserContextFactory::UsbChooserContextFactory()
    : ProfileKeyedServiceFactory(
          "UsbChooserContext",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

UsbChooserContextFactory::~UsbChooserContextFactory() = default;

std::unique_ptr<KeyedService>
UsbChooserContextFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<UsbChooserContext>(
      Profile::FromBrowserContext(context));
}

// static
UsbChooserContextFactory* UsbChooserContextFactory::GetInstance() {
  static base::NoDestructor<UsbChooserContextFactory> instance;
  return instance.get();
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
