// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/testing_pref_service_syncable.h"

#include <memory>

#include "base/functional/bind.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_value_store.h"
#include "testing/gtest/include/gtest/gtest.h"

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
        PrefNotifierImpl* pref_notifier)
    : sync_preferences::PrefServiceSyncable(
          std::unique_ptr<PrefNotifierImpl>(pref_notifier),
          std::make_unique<PrefValueStore>(managed_prefs.get(),
                                           supervised_user_prefs.get(),
                                           extension_prefs.get(),
                                           standalone_browser_prefs.get(),
                                           /*command_line_prefs=*/nullptr,
                                           user_prefs.get(),
                                           recommended_prefs.get(),
                                           pref_registry->defaults().get(),
                                           pref_notifier),
          user_prefs,
          standalone_browser_prefs,
          pref_registry,
          /*pref_model_associator_client=*/nullptr,
          base::BindRepeating(
              &TestingPrefServiceBase<
                  PrefServiceSyncable,
                  user_prefs::PrefRegistrySyncable>::HandleReadError),
          false),
      managed_prefs_(managed_prefs),
      supervised_user_prefs_(supervised_user_prefs),
      extension_prefs_(extension_prefs),
      standalone_browser_prefs_(standalone_browser_prefs),
      user_prefs_(user_prefs),
      recommended_prefs_(recommended_prefs) {}

namespace sync_preferences {

TestingPrefServiceSyncable::TestingPrefServiceSyncable()
    : TestingPrefServiceSyncable(
          /*managed_prefs=*/base::MakeRefCounted<TestingPrefStore>(),
          /*supervised_user_prefs=*/base::MakeRefCounted<TestingPrefStore>(),
          /*extension_prefs=*/base::MakeRefCounted<TestingPrefStore>(),
          /*standalone_browser_prefs=*/base::MakeRefCounted<TestingPrefStore>(),
          /*user_prefs=*/base::MakeRefCounted<TestingPrefStore>(),
          /*recommended_prefs=*/base::MakeRefCounted<TestingPrefStore>(),
          base::MakeRefCounted<user_prefs::PrefRegistrySyncable>(),
          std::make_unique<PrefNotifierImpl>()) {}

TestingPrefServiceSyncable::TestingPrefServiceSyncable(
    scoped_refptr<TestingPrefStore> managed_prefs,
    scoped_refptr<TestingPrefStore> supervised_user_prefs,
    scoped_refptr<TestingPrefStore> extension_prefs,
    scoped_refptr<TestingPrefStore> standalone_browser_prefs,
    scoped_refptr<TestingPrefStore> user_prefs,
    scoped_refptr<TestingPrefStore> recommended_prefs,
    scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry,
    std::unique_ptr<PrefNotifierImpl> pref_notifier)
    : TestingPrefServiceBase<PrefServiceSyncable,
                             user_prefs::PrefRegistrySyncable>(
          managed_prefs,
          supervised_user_prefs,
          extension_prefs,
          standalone_browser_prefs,
          user_prefs,
          recommended_prefs,
          pref_registry,
          pref_notifier.release()) {}

TestingPrefServiceSyncable::~TestingPrefServiceSyncable() = default;

user_prefs::PrefRegistrySyncable* TestingPrefServiceSyncable::registry() {
  return static_cast<user_prefs::PrefRegistrySyncable*>(
      DeprecatedGetPrefRegistry());
}

}  // namespace sync_preferences
