// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_SWITCHES_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_SWITCHES_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace password_manager {

#if BUILDFLAG(IS_LINUX)
extern const char kPasswordStore[];
extern const char kEnableEncryptionSelection[];
#endif

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_SWITCHES_H_
