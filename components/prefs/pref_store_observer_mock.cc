// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/pref_store_observer_mock.h"

#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"

PrefStoreObserverMock::PrefStoreObserverMock()
    : initialized(false), initialization_success(false) {}

PrefStoreObserverMock::~PrefStoreObserverMock() = default;

void PrefStoreObserverMock::VerifyAndResetChangedKey(
    const std::string& expected) {
  EXPECT_EQ(1u, changed_keys.size());
  if (changed_keys.size() >= 1)
    EXPECT_EQ(expected, changed_keys.front());
  changed_keys.clear();
}

void PrefStoreObserverMock::OnPrefValueChanged(std::string_view key) {
  changed_keys.emplace_back(key);
}

void PrefStoreObserverMock::OnInitializationCompleted(bool success) {
  initialized = true;
  initialization_success = success;
}
