// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_USER_INFO_H_
#define COMPONENTS_USER_MANAGER_USER_INFO_H_

#include <string>

#include "components/user_manager/user_manager_export.h"

class AccountId;

namespace gfx {
class ImageSkia;
}

namespace user_manager {

// A class that represents user related info.
class USER_MANAGER_EXPORT UserInfo {
 public:
  UserInfo();
  virtual ~UserInfo();

  // Gets the display name for the user.
  virtual std::u16string GetDisplayName() const = 0;

  // Gets the given name of the user.
  virtual std::u16string GetGivenName() const = 0;

  // Gets the display email address for the user.
  // The display email address might contains some periods in the email name
  // as well as capitalized letters. For example: "Foo.Bar@mock.com".
  virtual std::string GetDisplayEmail() const = 0;

  // Returns AccountId for the user.
  virtual const AccountId& GetAccountId() const = 0;

  // Gets the avatar image for the user.
  virtual const gfx::ImageSkia& GetImage() const = 0;
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_USER_INFO_H_
