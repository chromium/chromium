// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_CONSTANTS_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_CONSTANTS_H_

#define ACCOUNT_CAPABILITY(cpp_label, java_label, name) \
  extern const char cpp_label[];
#include "components/signin/internal/identity_manager/account_capabilities_list.h"
#undef ACCOUNT_CAPABILITY

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_CONSTANTS_H_
