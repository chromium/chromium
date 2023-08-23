// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_CONTENT_BROWSER_CONTEXT_KEYED_SERVICE_SHUTDOWN_NOTIFIER_FACTORY_H_
#define COMPONENTS_KEYED_SERVICE_CONTENT_BROWSER_CONTEXT_KEYED_SERVICE_SHUTDOWN_NOTIFIER_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service_export.h"

class KeyedServiceShutdownNotifier;

// A base class for factories for KeyedServiceShutdownNotifier objects that are
// keyed on a BrowserContext.
// To use this class, create a singleton subclass and declare its dependencies
// in the constructor.
class KEYED_SERVICE_EXPORT BrowserContextKeyedServiceShutdownNotifierFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  KeyedServiceShutdownNotifier* Get(content::BrowserContext* context);

  BrowserContextKeyedServiceShutdownNotifierFactory(
      const BrowserContextKeyedServiceShutdownNotifierFactory&) = delete;
  BrowserContextKeyedServiceShutdownNotifierFactory& operator=(
      const BrowserContextKeyedServiceShutdownNotifierFactory&) = delete;

 protected:
  explicit BrowserContextKeyedServiceShutdownNotifierFactory(const char* name);
  ~BrowserContextKeyedServiceShutdownNotifierFactory() override;

 private:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

#endif  // COMPONENTS_KEYED_SERVICE_CONTENT_BROWSER_CONTEXT_KEYED_SERVICE_SHUTDOWN_NOTIFIER_FACTORY_H_
