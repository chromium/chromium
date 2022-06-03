// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/scoped_user_manager.h"

#include <utility>

#include "base/check_op.h"
#include "components/user_manager/user_manager.h"

namespace user_manager {

ScopedUserManager::ScopedUserManager(std::unique_ptr<UserManager> user_manager)
    : user_manager_(std::move(user_manager)) {
  if (UserManager::GetForTesting())
    UserManager::GetForTesting()->Shutdown();

  previous_user_manager_ = UserManager::SetForTesting(user_manager_.get());
}

ScopedUserManager::~ScopedUserManager() {
  DCHECK_EQ(UserManager::Get(), user_manager_.get());

  // Shutdown and destroy current UserManager instance that we track.
  UserManager::Get()->Shutdown();
  UserManager::Get()->Destroy();

  UserManager::SetForTesting(previous_user_manager_);
}

}  // namespace user_manager
