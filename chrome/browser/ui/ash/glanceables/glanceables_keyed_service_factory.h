// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_KEYED_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {

class GlanceablesKeyedService;

// Factory class for `GlanceablesKeyedService`. Creates instances of that
// service for regular profiles only.
class GlanceablesKeyedServiceFactory : public ProfileKeyedServiceFactory {
 public:
  GlanceablesKeyedServiceFactory(const GlanceablesKeyedServiceFactory&) =
      delete;
  GlanceablesKeyedServiceFactory& operator=(
      const GlanceablesKeyedServiceFactory&) = delete;

  static GlanceablesKeyedServiceFactory* GetInstance();

  GlanceablesKeyedService* GetService(content::BrowserContext* context);

 protected:
  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

 private:
  friend base::NoDestructor<GlanceablesKeyedServiceFactory>;
  GlanceablesKeyedServiceFactory();
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_KEYED_SERVICE_FACTORY_H_
