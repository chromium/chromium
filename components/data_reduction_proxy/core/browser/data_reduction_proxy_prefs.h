// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_PREFS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

class PrefRegistrySimple;

namespace data_reduction_proxy {

// Registers the data reduction proxy's profile prefs on platforms that use
// syncable prefs.
void RegisterSyncableProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry);

// Registers the data reduction proxy's profile prefs on platforms that do not
// use syncable prefs.
void RegisterSimpleProfilePrefs(PrefRegistrySimple* registry);

// Registers local state, i.e., profile-agnostic prefs for the data
// reduction proxy.
void RegisterPrefs(PrefRegistrySimple* registry);

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_PREFS_H_
