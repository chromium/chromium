// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_DESKS_ADMIN_TEMPLATE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_ASH_DESKS_ADMIN_TEMPLATE_SERVICE_FACTORY_H_

#include "components/desks_storage/core/admin_template_service.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace desks_storage {
class AdminTemplateService;
}  // namespace desks_storage

namespace ash {

// Service factory that retrieves the AdminTemplateFactory for the appropriate
// BrowserContext.
class AdminTemplateServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static desks_storage::AdminTemplateService* GetForBrowserContext(
      content::BrowserContext* context);
  static AdminTemplateServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<AdminTemplateServiceFactory>;

  AdminTemplateServiceFactory();
  AdminTemplateServiceFactory(const AdminTemplateServiceFactory&) = delete;
  AdminTemplateServiceFactory& operator=(const AdminTemplateServiceFactory) =
      delete;
  ~AdminTemplateServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_DESKS_ADMIN_TEMPLATE_SERVICE_FACTORY_H_
