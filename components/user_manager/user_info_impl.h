// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_USER_INFO_IMPL_H_
#define COMPONENTS_USER_MANAGER_USER_INFO_IMPL_H_

#include <string>

#include "components/account_id/account_id.h"
#include "components/user_manager/user_info.h"
#include "components/user_manager/user_manager_export.h"
#include "ui/gfx/image/image_skia.h"

namespace user_manager {

// Stub implementation of UserInfo interface. Used in tests.
class USER_MANAGER_EXPORT UserInfoImpl : public UserInfo {
 public:
  UserInfoImpl();

  UserInfoImpl(const UserInfoImpl&) = delete;
  UserInfoImpl& operator=(const UserInfoImpl&) = delete;

  ~UserInfoImpl() override;

  // UserInfo:
  std::u16string GetDisplayName() const override;
  std::u16string GetGivenName() const override;
  std::string GetDisplayEmail() const override;
  const AccountId& GetAccountId() const override;
  const gfx::ImageSkia& GetImage() const override;

 private:
  const AccountId account_id_;
  gfx::ImageSkia user_image_;
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_USER_INFO_IMPL_H_
