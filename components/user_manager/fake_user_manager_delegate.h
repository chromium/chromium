// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_FAKE_USER_MANAGER_DELEGATE_H_
#define COMPONENTS_USER_MANAGER_FAKE_USER_MANAGER_DELEGATE_H_

#include "components/user_manager/user_manager_base.h"
#include "components/user_manager/user_manager_export.h"

namespace user_manager {

// Fake implementation of UserManagerBase::Delegate.
class USER_MANAGER_EXPORT FakeUserManagerDelegate
    : public UserManagerBase::Delegate {
 public:
  FakeUserManagerDelegate();
  FakeUserManagerDelegate(const FakeUserManagerDelegate&) = delete;
  FakeUserManagerDelegate& operator=(const FakeUserManagerDelegate&) = delete;
  ~FakeUserManagerDelegate() override;

  // UserManagerBase::Delegate:
  const std::string& GetApplicationLocale() override;
  void OverrideDirHome(const User& primary_user) override;
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_FAKE_USER_MANAGER_DELEGATE_H_
