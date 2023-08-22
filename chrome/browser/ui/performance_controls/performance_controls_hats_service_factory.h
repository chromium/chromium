// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_CONTROLS_HATS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_CONTROLS_HATS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"
#include "chrome/browser/ui/performance_controls/performance_controls_hats_service.h"

class PerformanceControlsHatsServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static PerformanceControlsHatsServiceFactory* GetInstance();
  static PerformanceControlsHatsService* GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<PerformanceControlsHatsServiceFactory>;

  PerformanceControlsHatsServiceFactory();
  ~PerformanceControlsHatsServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_CONTROLS_HATS_SERVICE_FACTORY_H_
