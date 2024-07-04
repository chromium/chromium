// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_CONTENT_BROWSER_CONTEXT_KEYED_SERVICE_FACTORY_H_
#define COMPONENTS_KEYED_SERVICE_CONTENT_BROWSER_CONTEXT_KEYED_SERVICE_FACTORY_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/keyed_service/core/keyed_service_export.h"
#include "components/keyed_service/core/keyed_service_factory.h"

class BrowserContextDependencyManager;
class KeyedService;

namespace content {
class BrowserContext;
}

// Base class for Factories that take a BrowserContext object and return some
// service on a one-to-one mapping. Barring unit tests, each factory that
// derives from this class *must* be a singleton (base::NoDestructor is
// recommended over base::Singleton).
//
// We do this because services depend on each other and we need to control
// shutdown/destruction order. In each derived classes' constructors, the
// implementors must explicitly state on which services they depend.
class KEYED_SERVICE_EXPORT BrowserContextKeyedServiceFactory
    : public KeyedServiceFactory {
 public:
  // A callback that supplies the instance of a KeyedService for a given
  // BrowserContext. This is used primarily for testing, where we want to feed
  // a specific test double into the BCKSF system.
  using TestingFactory = base::OnceCallback<std::unique_ptr<KeyedService>(
      content::BrowserContext* context)>;

  BrowserContextKeyedServiceFactory(const BrowserContextKeyedServiceFactory&) =
      delete;
  BrowserContextKeyedServiceFactory& operator=(
      const BrowserContextKeyedServiceFactory&) = delete;

  // Associates |testing_factory| with |context| so that |testing_factory| is
  // used to create the KeyedService when requested.  |testing_factory| can be
  // empty to signal that KeyedService should be null. Multiple calls to
  // SetTestingFactory() are allowed; previous services will be shut down.
  void SetTestingFactory(content::BrowserContext* context,
                         TestingFactory testing_factory);

  // Associates |testing_factory| with |context| and immediately returns the
  // created KeyedService. Since the factory will be used immediately, it may
  // not be empty.
  KeyedService* SetTestingFactoryAndUse(content::BrowserContext* context,
                                        TestingFactory testing_factory);

  // A variant of |TestingFactory| for supplying a subclass of
  // KeyedService for a given BrowserContext.
  template <typename Derived>
  using TestingSubclassFactory = base::OnceCallback<std::unique_ptr<Derived>(
      content::BrowserContext* context)>;

  // Like |SetTestingFactory|, but instead takes a factory for a subclass
  // of KeyedService and returns a pointer to this subclass. This allows
  // callers to avoid using static_cast in both directions: casting up to
  // KeyedService in their factory, and casting down to their subclass on
  // the returned pointer.
  template <typename Derived>
    requires(std::convertible_to<Derived*, KeyedService*>)
  Derived* SetTestingSubclassFactory(
      content::BrowserContext* context,
      TestingSubclassFactory<Derived> derived_factory) {
    TestingFactory upcast_factory = base::BindOnce(
        [](TestingSubclassFactory<Derived> derived_factory,
           content::BrowserContext* context) -> std::unique_ptr<KeyedService> {
          return std::move(derived_factory).Run(context);
        },
        std::move(derived_factory));

    return static_cast<Derived*>(
        SetTestingFactory(context, std::move(upcast_factory)));
  }

  // Like |SetTestingFactoryAndUse|, but instead takes a factory for a
  // subclass of KeyedService and returns a pointer to this subclass.
  // This allows callers to avoid using static_cast in both directions:
  // casting up to KeyedService in their factory, and casting down to
  // their subclass on the returned pointer.
  template <typename Derived>
    requires(std::convertible_to<Derived*, KeyedService*>)
  Derived* SetTestingSubclassFactoryAndUse(
      content::BrowserContext* context,
      TestingSubclassFactory<Derived> derived_factory) {
    TestingFactory upcast_factory = base::BindOnce(
        [](TestingSubclassFactory<Derived> derived_factory,
           content::BrowserContext* context) -> std::unique_ptr<KeyedService> {
          return std::move(derived_factory).Run(context);
        },
        std::move(derived_factory));

    return static_cast<Derived*>(
        SetTestingFactoryAndUse(context, std::move(upcast_factory)));
  }

 protected:
  // BrowserContextKeyedServiceFactories must communicate with a
  // BrowserContextDependencyManager. For all non-test code, write your subclass
  // constructors like this:
  //
  //   MyServiceFactory::MyServiceFactory()
  //     : BrowserContextKeyedServiceFactory(
  //         "MyService",
  //         BrowserContextDependencyManager::GetInstance())
  //   {}
  BrowserContextKeyedServiceFactory(const char* name,
                                    BrowserContextDependencyManager* manager);
  ~BrowserContextKeyedServiceFactory() override;

  // Common implementation that maps |context| to some service object. Deals
  // with incognito contexts per subclass instructions with
  // GetBrowserContextRedirectedInIncognito() and
  // GetBrowserContextOwnInstanceInIncognito() through the
  // GetBrowserContextToUse() method on the base.  If |create| is true, the
  // service will be created using BuildServiceInstanceFor() if it doesn't
  // already exist.
  KeyedService* GetServiceForBrowserContext(content::BrowserContext* context,
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
  //
  // Note: To ensure that this method takes effect, the Factory should be
  // instantiated before any BrowserContext is created to be part of the
  // dependency graph used to initialize all services on BrowserContext
  // creation.
  // The best practice is to initialize the factory in the appropriate
  // `EnsureBrowserContextKeyedServiceFactoriesBuilt()` method.
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
  //
  // Sub-classes implement one of these two forms:
  virtual std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const;

  // DEPRECATED: allows incremental conversion to the unique_ptr<> form
  // above. New code should not be using this form.
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const;

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

  // KeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      void* context) const final;

  // KeyedServiceBaseFactory:
  void* GetContextToUse(void* context) const final;
  bool ServiceIsCreatedWithContext() const final;
  void ContextShutdown(void* context) final;
  void ContextDestroyed(void* context) final;
  void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry) final;
  void CreateServiceNow(void* context) final;
};

#endif  // COMPONENTS_KEYED_SERVICE_CONTENT_BROWSER_CONTEXT_KEYED_SERVICE_FACTORY_H_
