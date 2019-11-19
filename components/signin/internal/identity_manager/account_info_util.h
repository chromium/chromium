// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_INFO_UTIL_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_INFO_UTIL_H_

#include "base/optional.h"
#include "base/values.h"
#include "components/signin/public/identity_manager/account_info.h"

// Builds an AccountInfo from the JSON data returned by the gaia servers (the
// data should have been converted to base::Value), if possible.
base::Optional<AccountInfo> AccountInfoFromUserInfo(
    const base::Value& user_info);

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_INFO_UTIL_H_
