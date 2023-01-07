// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_SCREENTIME_HISTORY_BRIDGE_FACTORY_H_
#define CHROME_BROWSER_UI_COCOA_SCREENTIME_HISTORY_BRIDGE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace screentime {

// A BrowserContextKeyedServiceFactory that is responsible for creating a
// HistoryBridge instance for each loaded Profile. The HistoryBridge instance is
// created when the Profile is initially created, so there's no explicit
// creation step.
class HistoryBridgeFactory : public ProfileKeyedServiceFactory {
 public:
  HistoryBridgeFactory();
  ~HistoryBridgeFactory() override;

  HistoryBridgeFactory(const HistoryBridgeFactory&) = delete;
  HistoryBridgeFactory& operator=(const HistoryBridgeFactory&) = delete;

  static HistoryBridgeFactory* GetInstance();
  static bool IsEnabled();

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace screentime

#endif  // CHROME_BROWSER_UI_COCOA_SCREENTIME_HISTORY_BRIDGE_FACTORY_H_
