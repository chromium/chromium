// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/pref_service_mock_factory.h"

#include "components/prefs/testing_pref_store.h"

namespace sync_preferences {
namespace {
scoped_refptr<TestingPrefStore> CreateTestingPrefStore() {
  scoped_refptr<TestingPrefStore> store =
      base::MakeRefCounted<TestingPrefStore>();
  // TestingPrefStore is initialized with read-only flag set to true by default.
  // This represents an error case in prod and is not effective for tests.
  // Moreoever, this can cause early-outs which avoids testing of some cases.
  store->set_read_only(false);
  return store;
}
}  // namespace

PrefServiceMockFactory::PrefServiceMockFactory() {
  set_user_prefs(CreateTestingPrefStore());
  SetAccountPrefStore(CreateTestingPrefStore());
}

PrefServiceMockFactory::~PrefServiceMockFactory() = default;

}  // namespace sync_preferences
