// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_METRICS_FACTORY_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_METRICS_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class Profile;

namespace web_app {

class WebAppMetrics;

// A factory to create WebAppMetrics. Its KeyedService shouldn't be created with
// browser context (forces the creation of SiteEngagementService otherwise).
class WebAppMetricsFactory : public BrowserContextKeyedServiceFactory {
 public:
  WebAppMetricsFactory(const WebAppMetricsFactory&) = delete;
  WebAppMetricsFactory& operator=(const WebAppMetricsFactory&) = delete;

  static WebAppMetrics* GetForProfile(Profile* profile);

  static WebAppMetricsFactory* GetInstance();

 private:
  friend base::NoDestructor<WebAppMetricsFactory>;

  WebAppMetricsFactory();
  ~WebAppMetricsFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_METRICS_FACTORY_H_
