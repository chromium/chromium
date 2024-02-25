// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/testing_pref_service.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "components/prefs/default_pref_store.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_value_store.h"
#include "testing/gtest/include/gtest/gtest.h"

template <>
TestingPrefServiceBase<PrefService, PrefRegistry>::TestingPrefServiceBase(
    scoped_refptr<TestingPrefStore> managed_prefs,
    scoped_refptr<TestingPrefStore> supervised_user_prefs,
    scoped_refptr<TestingPrefStore> extension_prefs,
    scoped_refptr<TestingPrefStore> standalone_browser_prefs,
    scoped_refptr<TestingPrefStore> user_prefs,
    scoped_refptr<TestingPrefStore> recommended_prefs,
    scoped_refptr<PrefRegistry> pref_registry,
    PrefNotifierImpl* pref_notifier)
    : PrefService(
          // Warning: `pref_notifier` is used for 2 arguments and the order of
          // computation isn't guaranteed. So making it a unique_ptr would cause
          // std::unique_ptr<>::get() after std::move().
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
          base::BindRepeating(
              &TestingPrefServiceBase<PrefService,
                                      PrefRegistry>::HandleReadError),
          false),
      managed_prefs_(managed_prefs),
      supervised_user_prefs_(supervised_user_prefs),
      extension_prefs_(extension_prefs),
      standalone_browser_prefs_(standalone_browser_prefs),
      user_prefs_(user_prefs),
      recommended_prefs_(recommended_prefs) {}

TestingPrefServiceSimple::TestingPrefServiceSimple()
    : TestingPrefServiceBase<PrefService, PrefRegistry>(
          /*managed_prefs=*/base::MakeRefCounted<TestingPrefStore>(),
          /*supervised_user_prefs=*/base::MakeRefCounted<TestingPrefStore>(),
          /*extension_prefs=*/base::MakeRefCounted<TestingPrefStore>(),
          /*standalone_browser_prefs=*/base::MakeRefCounted<TestingPrefStore>(),
          /*user_prefs=*/base::MakeRefCounted<TestingPrefStore>(),
          /*recommended_prefs=*/base::MakeRefCounted<TestingPrefStore>(),
          base::MakeRefCounted<PrefRegistrySimple>(),
          new PrefNotifierImpl()) {}

TestingPrefServiceSimple::~TestingPrefServiceSimple() {
}

PrefRegistrySimple* TestingPrefServiceSimple::registry() {
  return static_cast<PrefRegistrySimple*>(DeprecatedGetPrefRegistry());
}
