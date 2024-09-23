// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_service_factory.h"

#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_service.h"

// static
ReadAnythingService* ReadAnythingServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ReadAnythingService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ReadAnythingServiceFactory* ReadAnythingServiceFactory::GetInstance() {
  static base::NoDestructor<ReadAnythingServiceFactory> instance;
  return instance.get();
}

ReadAnythingServiceFactory::ReadAnythingServiceFactory()
    : ProfileKeyedServiceFactory(
          "ReadAnythingServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
}

bool ReadAnythingServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

std::unique_ptr<KeyedService>
ReadAnythingServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ReadAnythingService>(
      Profile::FromBrowserContext(context));
}
