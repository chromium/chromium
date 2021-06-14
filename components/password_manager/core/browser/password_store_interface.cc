// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_interface.h"
#include "base/notreached.h"

namespace password_manager {

void PasswordStoreInterface::Observer::OnLoginsChangedIn(
    PasswordStoreInterface* store,
    const PasswordStoreChangeList& changes) {
  OnLoginsChanged(changes);
}

void PasswordStoreInterface::Observer::OnLoginsRetained(
    const std::vector<PasswordForm>& retained_passwords) {
  // TODO(crbug.com/1217070): Make sure observers use this notification when
  // it is emitted.
  NOTIMPLEMENTED();
}

}  // namespace password_manager
