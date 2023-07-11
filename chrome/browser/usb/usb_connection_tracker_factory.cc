// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_connection_tracker_factory.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"

// static
UsbConnectionTrackerFactory* UsbConnectionTrackerFactory::GetInstance() {
  static base::NoDestructor<UsbConnectionTrackerFactory> factory;
  return factory.get();
}

// static
UsbConnectionTracker* UsbConnectionTrackerFactory::GetForProfile(
    Profile* profile,
    bool create) {
  return static_cast<UsbConnectionTracker*>(
      GetInstance()->GetServiceForBrowserContext(profile, create));
}

UsbConnectionTrackerFactory::UsbConnectionTrackerFactory()
    : ProfileKeyedServiceFactory(
          "UsbConnectionTracker",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

UsbConnectionTrackerFactory::~UsbConnectionTrackerFactory() = default;

KeyedService* UsbConnectionTrackerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new UsbConnectionTracker(Profile::FromBrowserContext(context));
}

void UsbConnectionTrackerFactory::BrowserContextShutdown(
    content::BrowserContext* context) {
  DCHECK(context);
  auto* device_connection_tracker =
      GetForProfile(Profile::FromBrowserContext(context), /*create=*/false);
  if (device_connection_tracker) {
    device_connection_tracker->CleanUp();
  }
  ProfileKeyedServiceFactory::BrowserContextShutdown(context);
}
