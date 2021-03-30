// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/insecure_credentials_observer.h"

#include <memory>

#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "components/password_manager/core/browser/insecure_credentials_table.h"

namespace password_manager {

void ProcessLoginsChanged(const PasswordStoreChangeList& changes,
                          const RemoveInsecureCallback& remove_callback) {
  for (const PasswordStoreChange& change : changes) {
    // New passwords are not interesting.
    if (change.type() == PasswordStoreChange::ADD)
      continue;
    // Updates are interesting only when they change the password value.
    if (change.type() == PasswordStoreChange::UPDATE &&
        !change.password_changed())
      continue;
    auto reason = RemoveInsecureCredentialsReason::kUpdate;
    if (change.type() == PasswordStoreChange::REMOVE &&
        base::ranges::none_of(changes, [](const auto& change) {
          return change.type() == PasswordStoreChange::ADD;
        })) {
      reason = RemoveInsecureCredentialsReason::kRemove;
    }
    remove_callback.Run(change.form().signon_realm,
                        change.form().username_value, reason);
    UMA_HISTOGRAM_ENUMERATION("PasswordManager.RemoveCompromisedCredentials",
                              reason == RemoveInsecureCredentialsReason::kUpdate
                                  ? PasswordStoreChange::UPDATE
                                  : PasswordStoreChange::REMOVE);
  }
}

}  // namespace password_manager
