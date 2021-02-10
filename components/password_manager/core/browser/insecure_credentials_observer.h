// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_INSECURE_CREDENTIALS_OBSERVER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_INSECURE_CREDENTIALS_OBSERVER_H_

#include "base/callback.h"
#include "components/password_manager/core/browser/insecure_credentials_table.h"
#include "components/password_manager/core/browser/password_store_change.h"

namespace password_manager {

// The callback to use to synchronously remove a insecure credentials from the
// password store. The parameters match those in RemoveInsecureCredentials.
using RemoveInsecureCallback =
    base::RepeatingCallback<void(const std::string&,
                                 const base::string16&,
                                 RemoveInsecureCredentialsReason)>;

// Called when the content of the password store changes.
// Removes rows from the insecure credentials database if the login was removed
// or the password was updated. If row is not in the database, the call is
// ignored.
void ProcessLoginsChanged(const PasswordStoreChangeList& changes,
                          const RemoveInsecureCallback& remove_callback);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_INSECURE_CREDENTIALS_OBSERVER_H_
