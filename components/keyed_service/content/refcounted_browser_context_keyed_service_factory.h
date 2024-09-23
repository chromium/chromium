// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_CONTENT_REFCOUNTED_BROWSER_CONTEXT_KEYED_SERVICE_FACTORY_H_
#define COMPONENTS_KEYED_SERVICE_CONTENT_REFCOUNTED_BROWSER_CONTEXT_KEYED_SERVICE_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "components/keyed_service/core/keyed_service_export.h"
#include "components/keyed_service/core/refcounted_keyed_service_factory.h"

class BrowserContextDependencyManager;
class RefcountedKeyedService;

namespace content {
class BrowserContext;
}

// A specialized BrowserContextKeyedServiceFactory that manages a
// RefcountedThreadSafe<>.
//
// While the factory returns RefcountedThreadSafe<>s, the factory itself is a
// base::NotThreadSafe. Only call methods on this object on the UI thread.
//
// Implementers of RefcountedKeyedService should note that we guarantee that
// ShutdownOnUIThread() is called on the UI thread, but actual object
// destruction can happen anywhere.
class KEYED_SERVICE_EXPORT RefcountedBrowserContextKeyedServiceFactory
    : public RefcountedKeyedServiceFactory {
 public:
  // A callback that supplies the instance of a KeyedService for a given
  // BrowserContext. This is used primarily for testing, where we want to feed
  // a specific test double into the BCKSF system.
  using TestingFactory =
      base::OnceCallback<scoped_refptr<RefcountedKeyedService>(
          content::BrowserContext* context)>;

  RefcountedBrowserContextKeyedServiceFactory(
      const RefcountedBrowserContextKeyedServiceFactory&) = delete;
  RefcountedBrowserContextKeyedServiceFactory& operator=(
      const RefcountedBrowserContextKeyedServiceFactory&) = delete;

  // Associates |testing_factory| with |context| so that |testing_factory| is
  // used to create the KeyedService when requested.  |testing_factory| can be
  // empty to signal that KeyedService should be null. Multiple calls to
  // SetTestingFactory() are allowed; previous services will be shut down.
  void SetTestingFactory(content::BrowserContext* context,
                         TestingFactory testing_factory);

  // Associates |testing_factory| with |context| and immediately returns the
  // created KeyedService. Since the factory will be used immediately, it may
  // not be empty.
  scoped_refptr<RefcountedKeyedService> SetTestingFactoryAndUse(
      content::BrowserContext* context,
      TestingFactory testing_factory);

 protected:
  // RefcountedBrowserContextKeyedServiceFactories must communicate with a
  // BrowserContextDependencyManager. For all non-test code, write your subclass
  // constructors like this:
  //
  //   MyServiceFactory::MyServiceFactory()
  //     : RefcountedBrowserContextKeyedServiceFactory(
  //         "MyService",
  //         BrowserContextDependencyManager::GetInstance())
  //   {}
  RefcountedBrowserContextKeyedServiceFactory(
      const char* name,
      BrowserContextDependencyManager* manager);
  ~RefcountedBrowserContextKeyedServiceFactory() override;

  // Common implementation that maps |context| to some service object. Deals
  // with incognito contexts per subclass instructions with
  // GetBrowserContextRedirectedInIncognito() and
  // GetBrowserContextOwnInstanceInIncognito() through the
  // GetBrowserContextToUse() method on the base.  If |create| is true, the
  // service will be created using BuildServiceInstanceFor() if it doesn't
  // already exist.
  scoped_refptr<RefcountedKeyedService> GetServiceForBrowserContext(
      content::BrowserContext* context,
      bool create);

  // Interface for people building a concrete FooServiceFactory: --------------

  // Finds which browser context (if any) to use.
  //
  // Should return nullptr when the service should not be created for the given
  // |context|.
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const;

  // By default, we create instances of a service lazily and wait until
  // GetForBrowserContext() is called on our subclass. Some services need to be
  // created as soon as the BrowserContext has been brought up.
  virtual bool ServiceIsCreatedWithBrowserContext() const;

  // By default, TestingBrowserContexts will be treated like normal contexts.
  // You can override this so that by default, the service associated with the
  // TestingBrowserContext is NULL. (This is just a shortcut around
  // SetTestingFactory() to make sure our contexts don't directly refer to the
  // services they use.)
  bool ServiceIsNULLWhileTesting() const override;

  // Interface for people building a type of BrowserContextKeyedFactory: -------

  // All subclasses of BrowserContextKeyedServiceFactory must return a
  // KeyedService instead of just a BrowserContextKeyedBase.
  //
  // This should not return nullptr; instead, return nullptr from
  // `GetBrowserContextToUse()`.
  virtual scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      content::BrowserContext* context) const = 0;

  // A helper object actually listens for notifications about BrowserContext
  // destruction, calculates the order in which things are destroyed and then
  // does a two pass shutdown.
  //
  // First, BrowserContextShutdown() is called on every ServiceFactory and will
  // usually call KeyedService::Shutdown(), which gives each
  // KeyedService a chance to remove dependencies on other
  // services that it may be holding.
  //
  // Secondly, BrowserContextDestroyed() is called on every ServiceFactory
  // and the default implementation removes it from |mapping_| and deletes
  // the pointer.
  virtual void BrowserContextShutdown(content::BrowserContext* context);
  virtual void BrowserContextDestroyed(content::BrowserContext* context);

 private:
  friend class BrowserContextDependencyManagerUnittests;

  // Registers any user preferences on this service. This should be overriden by
  // any service that wants to register profile-specific preferences.
  virtual void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) {}

  // RefcountedKeyedServiceFactory:
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      void* context) const final;

  // KeyedServiceBaseFactory:
  void* GetContextToUse(void* context) const final;
  bool ServiceIsCreatedWithContext() const final;
  void ContextShutdown(void* context) final;
  void ContextDestroyed(void* context) final;
  void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry) final;
};

#endif  // COMPONENTS_KEYED_SERVICE_CONTENT_REFCOUNTED_BROWSER_CONTEXT_KEYED_SERVICE_FACTORY_H_
