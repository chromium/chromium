// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_CORE_REFCOUNTED_KEYED_SERVICE_FACTORY_H_
#define COMPONENTS_KEYED_SERVICE_CORE_REFCOUNTED_KEYED_SERVICE_FACTORY_H_

#include <map>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/keyed_service/core/keyed_service_base_factory.h"
#include "components/keyed_service/core/keyed_service_export.h"

class RefcountedKeyedService;

// A specialized KeyedServiceBaseFactory that manages a RefcountedThreadSafe<>.
//
// While the factory returns RefcountedThreadSafe<>s, the factory itself is a
// base::NotThreadSafe. Only call methods on this object on the UI thread.
//
// Implementers of RefcountedKeyedService should note that we guarantee that
// ShutdownOnUIThread() is called on the UI thread, but actual object
// destruction can happen anywhere.
class KEYED_SERVICE_EXPORT RefcountedKeyedServiceFactory
    : public KeyedServiceBaseFactory {
 protected:
  RefcountedKeyedServiceFactory(const char* name,
                                DependencyManager* manager,
                                Type type);
  ~RefcountedKeyedServiceFactory() override;

  // A callback that supplies the instance of a KeyedService for a given
  // |context|. This is used primarily for testing, where we want to feed
  // a specific test double into the KeyedServiceFactory system.
  using TestingFactory =
      base::RepeatingCallback<scoped_refptr<RefcountedKeyedService>(
          void* context)>;

  // Associates |testing_factory| with |context| so that |testing_factory| is
  // used to create the KeyedService when requested.  |testing_factory| can be
  // empty to signal that KeyedService should be null.  Multiple calls to
  // SetTestingFactory() are allowed; previous services will be shut down.
  void SetTestingFactory(void* context, TestingFactory testing_factory);

  // Associates |testing_factory| with |context| and immediately returns the
  // created KeyedService. Since the factory will be used immediately, it may
  // not be empty.
  scoped_refptr<RefcountedKeyedService> SetTestingFactoryAndUse(
      void* context,
      TestingFactory testing_factory);

  // Common implementation that maps |context| to some service object. Deals
  // with incognito contexts per subclass instructions with GetContextToUse()
  // method on the base.  If |create| is true, the service will be created
  // using BuildServiceInstanceFor() if it doesn't already exist.
  scoped_refptr<RefcountedKeyedService> GetServiceForContext(void* context,
                                                             bool create);

  // Maps |context| to |service| with debug checks to prevent duplication and
  // returns |service|.
  scoped_refptr<RefcountedKeyedService> Associate(
      void* context,
      scoped_refptr<RefcountedKeyedService> service);

  // Removes the mapping from |context| to a service.
  void Disassociate(void* context);

  // Returns a new RefcountedKeyedService that will be associated with
  // |context|.
  virtual scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      void* context) const = 0;

  // Returns whether the |context| is off-the-record or not.
  virtual bool IsOffTheRecord(void* context) const = 0;

  // KeyedServiceBaseFactory:
  void ContextShutdown(void* context) override;
  void ContextDestroyed(void* context) override;

  void SetEmptyTestingFactory(void* context) override;
  bool HasTestingFactory(void* context) override;
  void CreateServiceNow(void* context) override;

 private:
  // The mapping between a context and its refcounted service.
  std::map<void*, scoped_refptr<RefcountedKeyedService>> mapping_;

  // The mapping between a context and its overridden TestingFactory.
  std::map<void*, TestingFactory> testing_factories_;

  DISALLOW_COPY_AND_ASSIGN(RefcountedKeyedServiceFactory);
};

#endif  // COMPONENTS_KEYED_SERVICE_CORE_REFCOUNTED_KEYED_SERVICE_FACTORY_H_
