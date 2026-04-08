// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_PASSWORD_REQUIREMENTS_SERVICE_FACTORY_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_PASSWORD_REQUIREMENTS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace password_manager {

class PasswordRequirementsService;

class PasswordRequirementsServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static PasswordRequirementsServiceFactory* GetInstance();

  // Returns the PasswordRequirementsService associated with |context|.
  // This may be nullptr for an incognito |context|.
  static PasswordRequirementsService* GetForBrowserContext(
      content::BrowserContext* context);

  PasswordRequirementsServiceFactory(
      const PasswordRequirementsServiceFactory&) = delete;
  PasswordRequirementsServiceFactory& operator=(
      const PasswordRequirementsServiceFactory&) = delete;

 private:
  friend base::NoDestructor<PasswordRequirementsServiceFactory>;

  PasswordRequirementsServiceFactory();
  ~PasswordRequirementsServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_PASSWORD_REQUIREMENTS_SERVICE_FACTORY_H_
