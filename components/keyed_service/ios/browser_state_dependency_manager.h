// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_IOS_BROWSER_STATE_DEPENDENCY_MANAGER_H_
#define COMPONENTS_KEYED_SERVICE_IOS_BROWSER_STATE_DEPENDENCY_MANAGER_H_

#include "base/callback_list.h"
#include "components/keyed_service/core/dependency_manager.h"
#include "components/keyed_service/core/keyed_service_export.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace web {
class BrowserState;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// A singleton that listens for context destruction notifications and
// rebroadcasts them to each KeyedServiceBaseFactory in a safe order
// based on the stated dependencies by each service.
class KEYED_SERVICE_EXPORT BrowserStateDependencyManager
    : public DependencyManager {
 public:
  static BrowserStateDependencyManager* GetInstance();

  BrowserStateDependencyManager(const BrowserStateDependencyManager&) = delete;
  BrowserStateDependencyManager& operator=(
      const BrowserStateDependencyManager&) = delete;

  // Registers context-specific preferences for all services via |registry|.
  void RegisterBrowserStatePrefsForServices(
      user_prefs::PrefRegistrySyncable* registry);

  // Called by each BrowserState to alert us of its creation. Service that
  // want to be started when BrowserState is created should override the
  // ServiceIsCreatedWithBrowserState() method in their factory. Preferences
  // registration also happens during that method call.
  void CreateBrowserStateServices(web::BrowserState* context);

  // Similar to CreateBrowserStateServices(), except this is used for creating
  // test BrowserStates - these contexts will not create services for any
  // BrowserStateKeyedBaseFactories that return true from
  // ServiceIsNULLWhileTesting().
  void CreateBrowserStateServicesForTest(web::BrowserState* context);

  // Called by each BrowserState to alert us that we should destroy services
  // associated with it.
  void DestroyBrowserStateServices(web::BrowserState* context);

  // Runtime assertion called as a part of GetServiceForBrowserState() to check
  // if |context| is considered stale. This will NOTREACHED() or
  // base::debug::DumpWithoutCrashing() depending on the DCHECK_IS_ON() value.
  void AssertBrowserStateWasntDestroyed(web::BrowserState* context) const;

  // Marks |context| as live (i.e., not stale). This method can be called as a
  // safeguard against |AssertBrowserStateWasntDestroyed()| checks going off
  // due to |context| aliasing a BrowserState instance from a prior construction
  // (i.e., 0xWhatever might be created, be destroyed, and then a new
  // BrowserState object might be created at 0xWhatever).
  void MarkBrowserStateLive(web::BrowserState* context);

 private:
  friend struct base::DefaultSingletonTraits<BrowserStateDependencyManager>;

  BrowserStateDependencyManager();
  ~BrowserStateDependencyManager() override;

  // Helper function used by CreateBrowserStateServices[ForTest].
  void DoCreateBrowserStateServices(web::BrowserState* context,
                                    bool is_testing_context);

#ifndef NDEBUG
  // DependencyManager:
  void DumpContextDependencies(void* context) const final;
#endif  // NDEBUG
};

#endif  // COMPONENTS_KEYED_SERVICE_IOS_BROWSER_STATE_DEPENDENCY_MANAGER_H_
