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
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

UsbConnectionTrackerFactory::~UsbConnectionTrackerFactory() = default;

std::unique_ptr<KeyedService>
UsbConnectionTrackerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<UsbConnectionTracker>(
      Profile::FromBrowserContext(context));
}
