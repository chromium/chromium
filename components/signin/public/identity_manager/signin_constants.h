// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_SIGNIN_CONSTANTS_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_SIGNIN_CONSTANTS_H_

namespace signin::constants {
// This must be a string which can never be a valid domain.
inline constexpr char kNoHostedDomainFound[] = "NO_HOSTED_DOMAIN";
}  // namespace signin::constants

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_SIGNIN_CONSTANTS_H_
