// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_finds_internals/chrome_finds_agent_factory.h"

#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_finds_internals/chrome_finds_agent.h"

namespace chrome_finds_internals {

// static
ChromeFindsAgent* ChromeFindsAgentFactory::GetForProfile(Profile* profile) {
  return static_cast<ChromeFindsAgent*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ChromeFindsAgentFactory* ChromeFindsAgentFactory::GetInstance() {
  static base::NoDestructor<ChromeFindsAgentFactory> instance;
  return instance.get();
}

ChromeFindsAgentFactory::ChromeFindsAgentFactory()
    : ProfileKeyedServiceFactory(
          "ChromeFindsAgent",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
}

ChromeFindsAgentFactory::~ChromeFindsAgentFactory() = default;

std::unique_ptr<KeyedService>
ChromeFindsAgentFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<ChromeFindsAgent>(
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile),
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS));
}

}  // namespace chrome_finds_internals
