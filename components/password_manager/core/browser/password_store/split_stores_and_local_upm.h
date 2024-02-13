// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_SPLIT_STORES_AND_LOCAL_UPM_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_SPLIT_STORES_AND_LOCAL_UPM_H_

class PrefService;

namespace password_manager {

//  Checks whether the UPM for local users is activated for this client.
//  This also means that the single password store has been split in
//  account and local stores.
bool UsesSplitStoresAndUPMForLocal(PrefService* pref_service);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_SPLIT_STORES_AND_LOCAL_UPM_H_
