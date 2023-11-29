// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CUSTOM_HANDLERS_SIMPLE_PROTOCOL_HANDLER_REGISTRY_FACTORY_H_
#define COMPONENTS_CUSTOM_HANDLERS_SIMPLE_PROTOCOL_HANDLER_REGISTRY_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace custom_handlers {

class ProtocolHandlerRegistry;

// Simgleton that owns all the ProtocolHandlerRegistrys and associates them with
// BrowserContext instances.
//
// It creates the Registry instances without access to the PrefService storage.
// This is useful for testing purposes, since we can't be sure the UserPref
// storage has been set (eg, Web Platform Tests).
//
// It uses the TestProtocolHandlerRegistryDelegate, hence it doesn't provide any
// OS integration during the registration process.
//
// It does not override the GetBrowserContextToUse method from
// BrowserContextKeyedServiceFactory, which means that no service is returned in
// Incognito.
class SimpleProtocolHandlerRegistryFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the singleton instance of the ProtocolHandlerRegistryFactory.
  static SimpleProtocolHandlerRegistryFactory* GetInstance();

  // Returns the ProtocolHandlerRegistry that provides intent registration for
  // |context|. Ownership stays with this factory object.
  // Allows the caller to indicate that the KeyedService should not be created
  // if it's not registered. This is particularly useful for testings purposes,
  // since the TestBrowserContext doesn't implement the TwoPhaseShutdown of
  // KeyedService instances.
  static ProtocolHandlerRegistry* GetForBrowserContext(
      content::BrowserContext* context,
      bool create = false);

  SimpleProtocolHandlerRegistryFactory(
      const SimpleProtocolHandlerRegistryFactory&) = delete;
  SimpleProtocolHandlerRegistryFactory& operator=(
      const SimpleProtocolHandlerRegistryFactory&) = delete;

 protected:
  // BrowserContextKeyedServiceFactory implementation.
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

 private:
  friend class base::NoDestructor<SimpleProtocolHandlerRegistryFactory>;

  SimpleProtocolHandlerRegistryFactory();
  ~SimpleProtocolHandlerRegistryFactory() override = default;

  // BrowserContextKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace custom_handlers

#endif  // COMPONENTS_CUSTOM_HANDLERS_SIMPLE_PROTOCOL_HANDLER_REGISTRY_FACTORY_H_
