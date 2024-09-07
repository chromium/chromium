// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class ReadAnythingService;

// See ReadAnythingService for details.
// A service is built for regular and incognito profiles, but not guest, system
// or other irregular profiles.
class ReadAnythingServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ReadAnythingService* GetForBrowserContext(
      content::BrowserContext* context);
  static ReadAnythingServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<ReadAnythingServiceFactory>;

  ReadAnythingServiceFactory();

  // BrowserContextKeyedServiceFactory implementation:
  bool ServiceIsCreatedWithBrowserContext() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_SERVICE_FACTORY_H_
