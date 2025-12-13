// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WAAP_WAAP_UI_METRICS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_WAAP_WAAP_UI_METRICS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class WaapUIMetricsService;

// The factory is a singleton that owns all WaapUIMetricsService and associates
// them with Profiles.
// It cleans up the associated WaapUIMetricsService when the corresponding
// Profile is destroyed.
class WaapUIMetricsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the instance of WaapUIMetricsService associated with this
  // profile (creating one if none exists).
  // This should return non-nullptr for all kinds of profile as this service is
  // needed for reporting metrics in any conditions.
  static WaapUIMetricsService* GetForProfile(Profile* profile);

  // Returns the instance of WaapUIMetricsServiceFactory.
  static WaapUIMetricsServiceFactory* GetInstance();

  WaapUIMetricsServiceFactory(const WaapUIMetricsServiceFactory&) = delete;
  WaapUIMetricsServiceFactory& operator=(const WaapUIMetricsServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<WaapUIMetricsServiceFactory>;

  WaapUIMetricsServiceFactory();
  ~WaapUIMetricsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_UI_WAAP_WAAP_UI_METRICS_SERVICE_FACTORY_H_
