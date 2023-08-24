// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/glanceables/glanceables_keyed_service_factory.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/ui/ash/glanceables/glanceables_keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace ash {

// static
GlanceablesKeyedServiceFactory* GlanceablesKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<GlanceablesKeyedServiceFactory> factory;
  return factory.get();
}

GlanceablesKeyedServiceFactory::GlanceablesKeyedServiceFactory()
    : ProfileKeyedServiceFactory("GlanceablesKeyedService",
                                 ProfileSelections::BuildForRegularProfile()) {}

GlanceablesKeyedService* GlanceablesKeyedServiceFactory::GetService(
    content::BrowserContext* context) {
  return static_cast<GlanceablesKeyedService*>(
      GetInstance()->GetServiceForBrowserContext(
          context, /*create=*/features::AreGlanceablesV2Enabled() ||
                       features::AreGlanceablesV2EnabledForTrustedTesters()));
}

std::unique_ptr<KeyedService>
GlanceablesKeyedServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<GlanceablesKeyedService>(
      Profile::FromBrowserContext(context));
}

}  // namespace ash
