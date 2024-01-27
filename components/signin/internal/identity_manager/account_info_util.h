// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_INFO_UTIL_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_INFO_UTIL_H_

#include <optional>

#include "base/values.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_info.h"

// Builds an AccountInfo from the JSON data returned by the gaia servers (the
// data should have been converted to base::Value::Dict), if possible.
std::optional<AccountInfo> AccountInfoFromUserInfo(
    const base::Value::Dict& user_info);

// Builds an AccountCapabilities from the JSON data returned by the server,
// if possible.
std::optional<AccountCapabilities> AccountCapabilitiesFromValue(
    const base::Value::Dict& account_capabilities);

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_INFO_UTIL_H_
