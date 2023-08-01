// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/screentime/history_bridge_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cocoa/screentime/history_bridge.h"
#include "chrome/browser/ui/cocoa/screentime/history_deleter_impl.h"
#include "chrome/browser/ui/cocoa/screentime/screentime_features.h"

namespace screentime {

// static
HistoryBridgeFactory* HistoryBridgeFactory::GetInstance() {
  static base::NoDestructor<HistoryBridgeFactory> factory;
  return factory.get();
}

HistoryBridgeFactory::HistoryBridgeFactory()
    : ProfileKeyedServiceFactory("screentime::HistoryBridge") {}
HistoryBridgeFactory::~HistoryBridgeFactory() = default;

// static
bool HistoryBridgeFactory::IsEnabled() {
  return base::FeatureList::IsEnabled(kScreenTime);
}

KeyedService* HistoryBridgeFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);
  auto* service = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::IMPLICIT_ACCESS);

  auto deleter = HistoryDeleterImpl::Create();

  return new HistoryBridge(service, std::move(deleter));
}

bool HistoryBridgeFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool HistoryBridgeFactory::ServiceIsNULLWhileTesting() const {
  // Never create a HistoryBridge for a test context. They will always end up
  // backed by a real HistoryDeleterImpl, which will try to talk to the system
  // ScreenTime service, which will either make the test very slow or introduce
  // flake. Tests need to explicitly opt into having real ScreenTime when they
  // want it.
  return true;
}

}  // namespace screentime
