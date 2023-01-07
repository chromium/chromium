// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/account_capabilities_constants.h"

#define ACCOUNT_CAPABILITY(cpp_label, java_label, name) \
  const char cpp_label[] = name;
#include "components/signin/internal/identity_manager/account_capabilities_list.h"
#undef ACCOUNT_CAPABILITY
