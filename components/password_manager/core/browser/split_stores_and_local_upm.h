// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SPLIT_STORES_AND_LOCAL_UPM_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SPLIT_STORES_AND_LOCAL_UPM_H_

// TODO(crbug.com/442347616): Rename this file and the test fixture. Ideally
// IsPasswordManagerAvailable() should live next to these methods and they would
// all be named consistently.

namespace password_manager {

// Returns if it is a requirement to update the GMSCore based on whether split
// password stores are supported or not.
bool IsGmsCoreUpdateRequired();

// The min GMS version which supports the account/local password separation.
// This is a exposed as a function because the value is different on auto /
// non-auto and the form factor can only be checked in runtime.
int GetSplitStoresUpmMinVersion();

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SPLIT_STORES_AND_LOCAL_UPM_H_
