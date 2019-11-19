// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/testing_pref_service.h"

#include <memory>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "components/prefs/default_pref_store.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_value_store.h"
#include "testing/gtest/include/gtest/gtest.h"

template <>
TestingPrefServiceBase<PrefService, PrefRegistry>::TestingPrefServiceBase(
    TestingPrefStore* managed_prefs,
    TestingPrefStore* extension_prefs,
    TestingPrefStore* user_prefs,
    TestingPrefStore* recommended_prefs,
    PrefRegistry* pref_registry,
    PrefNotifierImpl* pref_notifier)
    : PrefService(
          std::unique_ptr<PrefNotifierImpl>(pref_notifier),
          std::make_unique<PrefValueStore>(managed_prefs,
                                           nullptr,
                                           extension_prefs,
                                           nullptr,
                                           user_prefs,
                                           recommended_prefs,
                                           pref_registry->defaults().get(),
                                           pref_notifier),
          user_prefs,
          pref_registry,
          base::BindRepeating(
              &TestingPrefServiceBase<PrefService,
                                      PrefRegistry>::HandleReadError),
          false),
      managed_prefs_(managed_prefs),
      extension_prefs_(extension_prefs),
      user_prefs_(user_prefs),
      recommended_prefs_(recommended_prefs) {}

TestingPrefServiceSimple::TestingPrefServiceSimple()
    : TestingPrefServiceBase<PrefService, PrefRegistry>(
          new TestingPrefStore(),
          new TestingPrefStore(),
          new TestingPrefStore(),
          new TestingPrefStore(),
          new PrefRegistrySimple(),
          new PrefNotifierImpl()) {}

TestingPrefServiceSimple::~TestingPrefServiceSimple() {
}

PrefRegistrySimple* TestingPrefServiceSimple::registry() {
  return static_cast<PrefRegistrySimple*>(DeprecatedGetPrefRegistry());
}
