// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/cross_device_pref_tracker/common_cross_device_pref_provider.h"

#include "base/no_destructor.h"

namespace sync_preferences {

namespace {

// Helper to return a common, static empty set.
const base::flat_set<std::string_view>& GetEmptySet() {
  static const base::NoDestructor<base::flat_set<std::string_view>> kEmptySet;
  return *kEmptySet;
}

}  // namespace

CommonCrossDevicePrefProvider::CommonCrossDevicePrefProvider() = default;
CommonCrossDevicePrefProvider::~CommonCrossDevicePrefProvider() = default;

const base::flat_set<std::string_view>&
CommonCrossDevicePrefProvider::GetProfilePrefs() const {
  return GetEmptySet();
}

const base::flat_set<std::string_view>&
CommonCrossDevicePrefProvider::GetLocalStatePrefs() const {
  return GetEmptySet();
}

}  // namespace sync_preferences
