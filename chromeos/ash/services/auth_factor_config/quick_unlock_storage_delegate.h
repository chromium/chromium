// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_QUICK_UNLOCK_STORAGE_DELEGATE_H_
#define CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_QUICK_UNLOCK_STORAGE_DELEGATE_H_

#include <memory>
#include <string>
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/user_manager/user.h"

namespace ash::auth {

class QuickUnlockStorageDelegate {
 public:
  QuickUnlockStorageDelegate() = default;
  QuickUnlockStorageDelegate(const QuickUnlockStorageDelegate&) = delete;
  QuickUnlockStorageDelegate& operator=(const QuickUnlockStorageDelegate&) =
      delete;
  virtual ~QuickUnlockStorageDelegate() = default;

  virtual UserContext* GetUserContext(const ::user_manager::User* user,
                                      const std::string& token) = 0;
  virtual void SetUserContext(const ::user_manager::User* user,
                              std::unique_ptr<UserContext> user_context) = 0;
};

}  // namespace ash::auth

#endif  // CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_QUICK_UNLOCK_STORAGE_DELEGATE_H_
