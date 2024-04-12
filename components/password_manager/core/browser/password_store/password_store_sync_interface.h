// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_SYNC_INTERFACE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_SYNC_INTERFACE_H_

#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "build/buildflag.h"
#include "components/password_manager/core/browser/password_form.h"

namespace password_manager {

// Interface for the integration between the password store and the sync
// service.
class PasswordStoreSyncInterface {
 public:
  virtual base::CallbackListSubscription AddSyncEnabledOrDisabledCallback(
      base::RepeatingClosure sync_enabled_or_disabled_cb) = 0;

#if !BUILDFLAG(IS_ANDROID)
  virtual void GetUnsyncedCredentials(
      base::OnceCallback<void(std::vector<PasswordForm>)> callback) = 0;
#endif  // !BUILDFLAG(OS_ANDROID)

 protected:
  virtual ~PasswordStoreSyncInterface() = default;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_SYNC_INTERFACE_H_
