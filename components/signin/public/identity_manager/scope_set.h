// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_SCOPE_SET_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_SCOPE_SET_H_

#include <set>
#include <string>

namespace signin {

// TODO(crbug.com/425896213): Remove this file after migration complete.
using ScopeSet = std::set<std::string>;

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_SCOPE_SET_H_
