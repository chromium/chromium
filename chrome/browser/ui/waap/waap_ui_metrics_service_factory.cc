// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/waap_ui_metrics_service_factory.h"

#include "base/feature_list.h"
#include "base/types/pass_key.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/waap/waap_ui_metrics_service.h"
#include "chrome/common/chrome_features.h"

// static
WaapUIMetricsService* WaapUIMetricsServiceFactory::GetForProfile(
    Profile* profile) {
  if (!profile || !GetInstance()) {
    return nullptr;
  }
  return static_cast<WaapUIMetricsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
WaapUIMetricsServiceFactory* WaapUIMetricsServiceFactory::GetInstance() {
  static base::NoDestructor<WaapUIMetricsServiceFactory> instance;

  return base::FeatureList::IsEnabled(features::kInitialWebUIMetrics)
             ? instance.get()
             : nullptr;
}

WaapUIMetricsServiceFactory::WaapUIMetricsServiceFactory()
    : ProfileKeyedServiceFactory(
          "WaapUIMetricsService",
          ProfileSelections::Builder()
              // Regular & Incognito profiles.
              .WithRegular(ProfileSelection::kOwnInstance)
              // Guest profiles.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {}

WaapUIMetricsServiceFactory::~WaapUIMetricsServiceFactory() = default;

std::unique_ptr<KeyedService>
WaapUIMetricsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<WaapUIMetricsService>(
      base::PassKey<WaapUIMetricsServiceFactory>(), profile);
}
