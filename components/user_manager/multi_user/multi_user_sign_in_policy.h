// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_MULTI_USER_MULTI_USER_SIGN_IN_POLICY_H_
#define COMPONENTS_USER_MANAGER_MULTI_USER_MULTI_USER_SIGN_IN_POLICY_H_

namespace user_manager {

enum class MultiUserSignInPolicy {
  kUnrestricted = 0,
  kPrimaryOnly = 1,
  kNotAllowed = 2,
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_MULTI_USER_MULTI_USER_SIGN_IN_POLICY_H_
