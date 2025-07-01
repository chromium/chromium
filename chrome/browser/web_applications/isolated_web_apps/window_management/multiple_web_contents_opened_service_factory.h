// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WINDOW_MANAGEMENT_MULTIPLE_WEB_CONTENTS_OPENED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WINDOW_MANAGEMENT_MULTIPLE_WEB_CONTENTS_OPENED_SERVICE_FACTORY_H_

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/web_applications/isolated_web_apps/window_management/multiple_web_contents_opened_service.h"

class MultipleWebContentsOpenedServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static MultipleWebContentsOpenedServiceFactory* GetInstance();
  static MultipleWebContentsOpenedService* GetForProfile(Profile* profile);

 protected:
  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* browser_context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;

 private:
  friend base::NoDestructor<MultipleWebContentsOpenedServiceFactory>;

  MultipleWebContentsOpenedServiceFactory();
  ~MultipleWebContentsOpenedServiceFactory() override = default;
};

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WINDOW_MANAGEMENT_MULTIPLE_WEB_CONTENTS_OPENED_SERVICE_FACTORY_H_
