// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEYS_DELETER_FACTORY_H_
#define COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEYS_DELETER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class KeyedService;

namespace content {

class BrowserContext;

}  // namespace content

namespace payments {

class BrowserBoundKeyDeleter;

// Responsible for creating a service to start the process of finding and
// deleting invalid browser bound key metadata metadata.
class BrowserBoundKeyDeleterFactory : public BrowserContextKeyedServiceFactory {
 public:
  static BrowserBoundKeyDeleterFactory* GetInstance();

  static BrowserBoundKeyDeleter* GetForBrowserContext(
      content::BrowserContext* context);

 protected:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  bool ServiceIsCreatedWithBrowserContext() const override;

 private:
  friend base::NoDestructor<BrowserBoundKeyDeleterFactory>;
  BrowserBoundKeyDeleterFactory();
  ~BrowserBoundKeyDeleterFactory() override;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEYS_DELETER_FACTORY_H_
