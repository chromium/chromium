// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_MOCK_PASSWORD_STORE_SYNC_INTERFACE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_MOCK_PASSWORD_STORE_SYNC_INTERFACE_H_

#include "build/buildflag.h"
#include "components/password_manager/core/browser/password_store/password_store_sync_interface.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

class MockPasswordStoreSyncInterface : public PasswordStoreSyncInterface {
 public:
  MockPasswordStoreSyncInterface();
  ~MockPasswordStoreSyncInterface() override;

  MOCK_METHOD(base::CallbackListSubscription,
              AddSyncEnabledOrDisabledCallback,
              (base::RepeatingClosure sync_enabled_or_disabled_cb),
              (override));

#if !BUILDFLAG(IS_ANDROID)
  MOCK_METHOD(void,
              GetUnsyncedCredentials,
              (base::OnceCallback<void(std::vector<PasswordForm>)>),
              (override));
#endif  // !BUILDFLAG(IS_ANDROID)
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_MOCK_PASSWORD_STORE_SYNC_INTERFACE_H_
