// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_IOS_REFCOUNTED_BROWSER_STATE_KEYED_SERVICE_FACTORY_H_
#define COMPONENTS_KEYED_SERVICE_IOS_REFCOUNTED_BROWSER_STATE_KEYED_SERVICE_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/keyed_service/core/keyed_service_export.h"
#include "components/keyed_service/core/refcounted_keyed_service_factory.h"

class BrowserStateDependencyManager;
class RefcountedKeyedService;

namespace web {
class BrowserState;
}

// A specialized BrowserStateKeyedServiceFactory that manages a
// RefcountedThreadSafe<>.
//
// While the factory returns RefcountedThreadSafe<>s, the factory itself is a
// base::NotThreadSafe. Only call methods on this object on the UI thread.
//
// Implementers of RefcountedKeyedService should note that we guarantee that
// ShutdownOnUIThread() is called on the UI thread, but actual object
// destruction can happen anywhere.
class KEYED_SERVICE_EXPORT RefcountedBrowserStateKeyedServiceFactory
    : public RefcountedKeyedServiceFactory {
 public:
  // A callback that supplies the instance of a KeyedService for a given
  // BrowserState. This is used primarily for testing, where we want to feed
  // a specific mock into the BSKSF system.
  using TestingFactory =
      base::RepeatingCallback<scoped_refptr<RefcountedKeyedService>(
          web::BrowserState* context)>;

  // Associates |testing_factory| with |context| so that |testing_factory| is
  // used to create the KeyedService when requested.  |testing_factory| can be
  // empty to signal that KeyedService should be null. Multiple calls to
  // SetTestingFactory() are allowed; previous services will be shut down.
  void SetTestingFactory(web::BrowserState* context,
                         TestingFactory testing_factory);

  // Associates |testing_factory| with |context| and immediately returns the
  // created KeyedService. Since the factory will be used immediately, it may
  // not be empty.
  scoped_refptr<RefcountedKeyedService> SetTestingFactoryAndUse(
      web::BrowserState* context,
      TestingFactory testing_factory);

 protected:
  // RefcountedBrowserStateKeyedServiceFactories must communicate with a
  // BrowserStateDependencyManager. For all non-test code, write your subclass
  // constructors like this:
  //
  //   MyServiceFactory::MyServiceFactory()
  //     : RefcountedBrowserStateKeyedServiceFactory(
  //         "MyService",
  //         BrowserStateDependencyManager::GetInstance()) {
  //   }
  RefcountedBrowserStateKeyedServiceFactory(
      const char* name,
      BrowserStateDependencyManager* manager);
  ~RefcountedBrowserStateKeyedServiceFactory() override;

  // Common implementation that maps |context| to some service object. Deals
  // with incognito and testing contexts per subclass instructions. If |create|
  // is true, the service will be created using BuildServiceInstanceFor() if it
  // doesn't already exist.
  scoped_refptr<RefcountedKeyedService> GetServiceForBrowserState(
      web::BrowserState* context,
      bool create);

  // Interface for people building a concrete FooServiceFactory: --------------

  // Finds which browser state (if any) to use.
  virtual web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const;

  // By default, we create instances of a service lazily and wait until
  // GetForBrowserState() is called on our subclass. Some services need to be
  // created as soon as the BrowserState has been brought up.
  virtual bool ServiceIsCreatedWithBrowserState() const;

  // By default, TestingBrowserStates will be treated like normal contexts.
  // You can override this so that by default, the service associated with the
  // TestingBrowserState is NULL. (This is just a shortcut around
  // SetTestingFactory() to make sure our contexts don't directly refer to the
  // services they use.)
  bool ServiceIsNULLWhileTesting() const override;

  // Interface for people building a type of BrowserStateKeyedFactory: -------

  // All subclasses of BrowserStateKeyedServiceFactory must return a
  // KeyedService instead of just a BrowserStateKeyedBase.
  virtual scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const = 0;

  // A helper object actually listens for notifications about BrowserState
  // destruction, calculates the order in which things are destroyed and then
  // does a two pass shutdown.
  //
  // First, BrowserStateShutdown() is called on every ServiceFactory and will
  // usually call KeyedService::Shutdown(), which gives each
  // KeyedService a chance to remove dependencies on other
  // services that it may be holding.
  //
  // Secondly, BrowserStateDestroyed() is called on every ServiceFactory
  // and the default implementation removes it from |mapping_| and deletes
  // the pointer.
  virtual void BrowserStateShutdown(web::BrowserState* context);
  virtual void BrowserStateDestroyed(web::BrowserState* context);

 private:
  // Registers any user preferences on this service. This should be overriden by
  // any service that wants to register profile-specific preferences.
  virtual void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) {}

  // RefcountedKeyedServiceFactory:
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      void* context) const final;
  bool IsOffTheRecord(void* context) const final;

  // KeyedServiceBaseFactory:
  void* GetContextToUse(void* context) const final;
  bool ServiceIsCreatedWithContext() const final;
  void ContextShutdown(void* context) final;
  void ContextDestroyed(void* context) final;
  void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry) final;

  DISALLOW_COPY_AND_ASSIGN(RefcountedBrowserStateKeyedServiceFactory);
};

#endif  // COMPONENTS_KEYED_SERVICE_IOS_REFCOUNTED_BROWSER_STATE_KEYED_SERVICE_FACTORY_H_
