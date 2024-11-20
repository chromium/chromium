// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_CONSTANTS_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_CONSTANTS_H_

// `inline` is important here: this ensures that even though the definition is
// in a header which can be included in multiple translation units, the linker
// will deduplicate them into a single definition.
#define ACCOUNT_CAPABILITY(cpp_label, java_label, name) \
  inline constexpr char cpp_label[] = name;
#include "components/signin/internal/identity_manager/account_capabilities_list.h"
#undef ACCOUNT_CAPABILITY

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_CONSTANTS_H_
