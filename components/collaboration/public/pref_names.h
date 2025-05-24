// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_PUBLIC_PREF_NAMES_H_
#define COMPONENTS_COLLABORATION_PUBLIC_PREF_NAMES_H_

class PrefRegistrySimple;

namespace collaboration::prefs {

// Whether to allow shared tab group features for managed account.
extern const char kSharedTabGroupsManagedAccountSetting[];

enum class SharedTabGroupsManagedAccountSetting {
  kEnabled = 0,
  kDisabled = 1,
};

// Registers user preferences related to permissions.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace collaboration::prefs

#endif  // COMPONENTS_COLLABORATION_PUBLIC_PREF_NAMES_H_
