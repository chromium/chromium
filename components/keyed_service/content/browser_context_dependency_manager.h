// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_CONTENT_BROWSER_CONTEXT_DEPENDENCY_MANAGER_H_
#define COMPONENTS_KEYED_SERVICE_CONTENT_BROWSER_CONTEXT_DEPENDENCY_MANAGER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/callback_list.h"
#include "base/macros.h"
#include "components/keyed_service/core/dependency_manager.h"
#include "components/keyed_service/core/keyed_service_export.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace content {
class BrowserContext;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// A singleton that listens for context destruction notifications and
// rebroadcasts them to each BrowserContextKeyedServiceFactory in a safe order
// based on the stated dependencies by each service.
class KEYED_SERVICE_EXPORT BrowserContextDependencyManager
    : public DependencyManager {
 public:
  // Registers profile-specific preferences for all services via |registry|.
  // |context| should be the BrowserContext containing |registry| and is used as
  // a key to prevent multiple registrations on the same BrowserContext in
  // tests.
  void RegisterProfilePrefsForServices(
      user_prefs::PrefRegistrySyncable* registry);

  // Called by each BrowserContext to alert us of its creation. Several
  // services want to be started when a context is created. If you want your
  // KeyedService to be started with the BrowserContext, override
  // BrowserContextKeyedServiceFactory::ServiceIsCreatedWithBrowserContext() to
  // return true. This method also registers any service-related preferences
  // for non-incognito profiles.
  void CreateBrowserContextServices(content::BrowserContext* context);

  // Similar to CreateBrowserContextServices(), except this is used for creating
  // test BrowserContexts - these contexts will not create services for any
  // BrowserContextKeyedServiceFactory that returns true from
  // ServiceIsNULLWhileTesting().
  void CreateBrowserContextServicesForTest(content::BrowserContext* context);

  // Called by each BrowserContext to alert us that we should destroy services
  // associated with it.
  void DestroyBrowserContextServices(content::BrowserContext* context);

  // Registers a |callback| that will be called just before executing
  // CreateBrowserContextServices() or CreateBrowserContextServicesForTest().
  // This can be useful in browser tests which wish to substitute test or mock
  // builders for the keyed services.
  std::unique_ptr<
      base::CallbackList<void(content::BrowserContext*)>::Subscription>
  RegisterWillCreateBrowserContextServicesCallbackForTesting(
      const base::Callback<void(content::BrowserContext*)>& callback);

  // Runtime assertion called as a part of GetServiceForBrowserContext() to
  // check if |context| is considered stale. This will NOTREACHED() or
  // base::debug::DumpWithoutCrashing() depending on the DCHECK_IS_ON() value.
  void AssertBrowserContextWasntDestroyed(
      content::BrowserContext* context) const;

  // Marks |context| as live (i.e., not stale). This method can be called as a
  // safeguard against |AssertBrowserContextWasntDestroyed()| checks going off
  // due to |context| aliasing a BrowserContext instance from a prior
  // construction (i.e., 0xWhatever might be created, be destroyed, and then a
  // new BrowserContext object might be created at 0xWhatever).
  void MarkBrowserContextLive(content::BrowserContext* context);

  static BrowserContextDependencyManager* GetInstance();

 private:
  friend class BrowserContextDependencyManagerUnittests;
  friend class base::NoDestructor<BrowserContextDependencyManager>;

  // Helper function used by CreateBrowserContextServices[ForTest].
  void DoCreateBrowserContextServices(content::BrowserContext* context,
                                      bool is_testing_context);

  BrowserContextDependencyManager();
  ~BrowserContextDependencyManager() override;

#ifndef NDEBUG
  // DependencyManager:
  void DumpContextDependencies(void* context) const final;
#endif  // NDEBUG

  // A list of callbacks to call just before executing
  // CreateBrowserContextServices() or CreateBrowserContextServicesForTest().
  base::CallbackList<void(content::BrowserContext*)>
      will_create_browser_context_services_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(BrowserContextDependencyManager);
};

#endif  // COMPONENTS_KEYED_SERVICE_CONTENT_BROWSER_CONTEXT_DEPENDENCY_MANAGER_H_
