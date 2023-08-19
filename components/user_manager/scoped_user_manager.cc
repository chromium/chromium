// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/scoped_user_manager.h"

#include <utility>

#include "base/check_op.h"
#include "components/user_manager/user_manager.h"

namespace user_manager::internal {

ScopedUserManagerImpl::ScopedUserManagerImpl() = default;
ScopedUserManagerImpl::~ScopedUserManagerImpl() = default;

void ScopedUserManagerImpl::Reset(std::unique_ptr<UserManager> user_manager) {
  if (user_manager_) {
    // This already overwrites the global UserManager, restore the original one.
    DCHECK_EQ(UserManager::Get(), user_manager_.get());
    user_manager_->Shutdown();
    user_manager_.reset();
    UserManager::SetForTesting(std::exchange(previous_user_manager_, nullptr));
  }

  DCHECK(!previous_user_manager_);
  if (user_manager) {
    previous_user_manager_ = UserManager::GetForTesting();
    if (previous_user_manager_) {
      previous_user_manager_->Shutdown();
    }
    user_manager_ = std::move(user_manager);
    UserManager::SetForTesting(user_manager_.get());
  }
}

}  // namespace user_manager::internal
