// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_PATHS_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_PATHS_H_

#include "build/build_config.h"

namespace policy {

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
extern const char kPolicyPath[];
#endif

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_PATHS_H_
