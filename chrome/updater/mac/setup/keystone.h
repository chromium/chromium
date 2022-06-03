// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_SETUP_KEYSTONE_H_
#define CHROME_UPDATER_MAC_SETUP_KEYSTONE_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

struct RegistrationRequest;

// Replaces a keystone installation with this updater's shims. Calls
// `register_callback` with data from Keystone's ticket store if needed.
// Returns true if conversion succeeded.
bool ConvertKeystone(UpdaterScope scope,
                     base::RepeatingCallback<void(const RegistrationRequest&)>
                         register_callback);

// Uninstalls Keystone from the system. Does not remove Keystone's ticket
// stores.
void UninstallKeystone(UpdaterScope scope);

namespace internal {

std::vector<RegistrationRequest> TicketsToMigrate(
    const std::string& ksadmin_tickets);

}  // namespace internal

}  // namespace updater

#endif  // CHROME_UPDATER_MAC_SETUP_KEYSTONE_H_
