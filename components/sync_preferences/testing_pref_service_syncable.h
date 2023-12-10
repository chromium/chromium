// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_TESTING_PREF_SERVICE_SYNCABLE_H_
#define COMPONENTS_SYNC_PREFERENCES_TESTING_PREF_SERVICE_SYNCABLE_H_

#include "base/memory/scoped_refptr.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/pref_service_syncable.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace sync_preferences {

// Test version of PrefServiceSyncable.
// This class hierarchy has a flaw: TestingPrefServiceBase is inheriting from
// the first template parameter (PrefServiceSyncable in this case). This means,
// all of the supported parameter types must support the same constructor
// signatures -- which they don't. Hence, it's not possible to properly inject
//  a PrefModelAssociatorClient.
// TODO(tschumann) The whole purpose of TestingPrefServiceBase is questionable
// and I'd be in favor of removing it completely:
//  -- it hides the dependency injection of the different stores
//  -- just to later offer ways to manipulate specific stores.
//  -- if tests just dependency injects the individual stores directly, they
//     already have full control and won't need that indirection at all.
// See PrefServiceSyncableMergeTest as an example of a cleaner way.
class TestingPrefServiceSyncable
    : public TestingPrefServiceBase<PrefServiceSyncable,
                                    user_prefs::PrefRegistrySyncable> {
 public:
  TestingPrefServiceSyncable();
  TestingPrefServiceSyncable(
      scoped_refptr<TestingPrefStore> managed_prefs,
      scoped_refptr<TestingPrefStore> supervised_user_prefs,
      scoped_refptr<TestingPrefStore> extension_prefs,
      scoped_refptr<TestingPrefStore> standalone_browser_prefs,
      scoped_refptr<TestingPrefStore> user_prefs,
      scoped_refptr<TestingPrefStore> recommended_prefs,
      scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry,
      std::unique_ptr<PrefNotifierImpl> pref_notifier);

  TestingPrefServiceSyncable(const TestingPrefServiceSyncable&) = delete;
  TestingPrefServiceSyncable& operator=(const TestingPrefServiceSyncable&) =
      delete;

  ~TestingPrefServiceSyncable() override;

  // This is provided as a convenience; on a production PrefService
  // you would do all registrations before constructing it, passing it
  // a PrefRegistry via its constructor (or via e.g. PrefServiceFactory).
  user_prefs::PrefRegistrySyncable* registry();
};

}  // namespace sync_preferences

template <>
TestingPrefServiceBase<sync_preferences::PrefServiceSyncable,
                       user_prefs::PrefRegistrySyncable>::
    TestingPrefServiceBase(
        scoped_refptr<TestingPrefStore> managed_prefs,
        scoped_refptr<TestingPrefStore> supervised_user_prefs,
        scoped_refptr<TestingPrefStore> extension_prefs,
        scoped_refptr<TestingPrefStore> standalone_browser_prefs,
        scoped_refptr<TestingPrefStore> user_prefs,
        scoped_refptr<TestingPrefStore> recommended_prefs,
        scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry,
        PrefNotifierImpl* pref_notifier);

#endif  // COMPONENTS_SYNC_PREFERENCES_TESTING_PREF_SERVICE_SYNCABLE_H_
