// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_SWITCHES_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_SWITCHES_H_

#include "build/build_config.h"

namespace password_manager {

#if BUILDFLAG(IS_LINUX)
extern const char kPasswordStore[];
extern const char kEnableEncryptionSelection[];
#endif  // BUILDFLAG(IS_LINUX)

extern const char kEnableShareButtonUnbranded[];

// Specifies the user data directory, which is where the browser will look for
// all of its state. Needs to be kept in sync with
// chrome/common/chrome_switches.h
inline constexpr char kUserDataDir[] = "user-data-dir";

// This switch allows testing password change feature on provided URL. Password
// change will be offered by submitting password form on any URL with matching
// eTLD+1.
inline constexpr char kPasswordChangeUrl[] = "password-change-url";

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_SWITCHES_H_
