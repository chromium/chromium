// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_PREF_STORE_OBSERVER_MOCK_H_
#define COMPONENTS_PREFS_PREF_STORE_OBSERVER_MOCK_H_

#include <string_view>
#include <vector>

#include "base/compiler_specific.h"
#include "components/prefs/pref_store.h"

// A mock implementation of PrefStore::Observer.
class PrefStoreObserverMock : public PrefStore::Observer {
 public:
  PrefStoreObserverMock();

  PrefStoreObserverMock(const PrefStoreObserverMock&) = delete;
  PrefStoreObserverMock& operator=(const PrefStoreObserverMock&) = delete;

  ~PrefStoreObserverMock() override;

  void VerifyAndResetChangedKey(const std::string& expected);

  // PrefStore::Observer implementation
  void OnPrefValueChanged(std::string_view key) override;
  void OnInitializationCompleted(bool success) override;

  std::vector<std::string> changed_keys;
  bool initialized;
  bool initialization_success;  // Only valid if |initialized|.
};

#endif  // COMPONENTS_PREFS_PREF_STORE_OBSERVER_MOCK_H_
