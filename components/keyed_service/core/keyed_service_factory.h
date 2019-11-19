// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_CORE_KEYED_SERVICE_FACTORY_H_
#define COMPONENTS_KEYED_SERVICE_CORE_KEYED_SERVICE_FACTORY_H_

#include <map>
#include <memory>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/keyed_service/core/keyed_service_base_factory.h"
#include "components/keyed_service/core/keyed_service_export.h"

class DependencyManager;
class KeyedService;

// Base class for Factories that take an opaque pointer and return some service
// on a one-to-one mapping. Each concrete factory that derives from this class
// *must* be a Singleton (only unit tests don't do that).
//
// We do this because services depend on each other and we need to control
// shutdown/destruction order. In each derived classes' constructors, the
// implementors must explicitly state which services are depended on.
class KEYED_SERVICE_EXPORT KeyedServiceFactory
    : public KeyedServiceBaseFactory {
 protected:
  KeyedServiceFactory(const char* name, DependencyManager* manager, Type type);
  ~KeyedServiceFactory() override;

  // A callback that supplies the instance of a KeyedService for a given
  // |context|. This is used primarily for testing, where we want to feed
  // a specific test double into the KeyedServiceFactory system.
  using TestingFactory =
      base::RepeatingCallback<std::unique_ptr<KeyedService>(void* context)>;

  // Associates |testing_factory| with |context| so that |testing_factory| is
  // used to create the KeyedService when requested.  |testing_factory| can be
  // empty to signal that KeyedService should be null.  Multiple calls to
  // SetTestingFactory() are allowed; previous services will be shut down.
  void SetTestingFactory(void* context, TestingFactory testing_factory);

  // Associates |testing_factory| with |context| and immediately returns the
  // created KeyedService. Since the factory will be used immediately, it may
  // not be empty.
  KeyedService* SetTestingFactoryAndUse(void* context,
                                        TestingFactory testing_factory);

  // Common implementation that maps |context| to some service object. Deals
  // with incognito contexts per subclass instructions with GetContextToUse()
  // method on the base.  If |create| is true, the service will be created
  // using BuildServiceInstanceFor() if it doesn't already exist.
  KeyedService* GetServiceForContext(void* context,
                                     bool create);

  // Maps |context| to |service| with debug checks to prevent duplication and
  // returns a raw pointer to |service|.
  KeyedService* Associate(void* context, std::unique_ptr<KeyedService> service);

  // Removes the mapping from |context| to a service.
  void Disassociate(void* context);

  // Returns a new KeyedService that will be associated with |context|. The
  // |side_parameter| could be nullptr or some object required to create a
  // service instance.
  virtual std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      void* context) const = 0;

  // Returns whether the |context| is off-the-record or not.
  virtual bool IsOffTheRecord(void* context) const = 0;

  // KeyedServiceBaseFactory:
  void ContextShutdown(void* context) override;
  void ContextDestroyed(void* context) override;

  void SetEmptyTestingFactory(void* context) override;
  bool HasTestingFactory(void* context) override;

 private:
  friend class DependencyManager;
  friend class DependencyManagerUnittests;

  // The mapping between a context and its service.
  std::map<void*, std::unique_ptr<KeyedService>> mapping_;

  // The mapping between a context and its overridden TestingFactory.
  std::map<void*, TestingFactory> testing_factories_;

  DISALLOW_COPY_AND_ASSIGN(KeyedServiceFactory);
};

#endif  // COMPONENTS_KEYED_SERVICE_CORE_KEYED_SERVICE_FACTORY_H_
