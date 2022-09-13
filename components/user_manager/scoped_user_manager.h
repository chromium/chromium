// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_SCOPED_USER_MANAGER_H_
#define COMPONENTS_USER_MANAGER_SCOPED_USER_MANAGER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/user_manager/user_manager_export.h"

namespace user_manager {

class UserManager;

// Helper class for unit tests. Initializes the UserManager singleton to the
// given |user_manager| and tears it down again on destruction. If the singleton
// had already been initialized, its previous value is restored after tearing
// down |user_manager|.
class USER_MANAGER_EXPORT ScopedUserManager {
 public:
  explicit ScopedUserManager(std::unique_ptr<UserManager> user_manager);

  ScopedUserManager(const ScopedUserManager&) = delete;
  ScopedUserManager& operator=(const ScopedUserManager&) = delete;

  ~ScopedUserManager();

 private:
  const std::unique_ptr<UserManager> user_manager_;
  raw_ptr<UserManager> previous_user_manager_ = nullptr;
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_SCOPED_USER_MANAGER_H_
