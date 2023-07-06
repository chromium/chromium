// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BROWSER_CONTEXT_HELPER_BROWSER_CONTEXT_TYPES_H_
#define CHROMEOS_ASH_COMPONENTS_BROWSER_CONTEXT_HELPER_BROWSER_CONTEXT_TYPES_H_

// Ash has four special BrowserContext (a.k.a. Profile) types as follows:
// 1) Signin browser context, which is used on login screen.
// 2) Lock-screen-app browser context, which is used for launching platform
//    apps that can display windows on top of the lock screen.
// 3) Lock-screen browser context, which is used during online authentication
//    on the lock screen.
// 4) Shimless-rma-app browser context, which is used for launching 3p
//    diagnostics apps on shimless rma screen.
//
// This file provides convenient utilities to check those.
// In order to obtain a BrowserContext instance for those, please take a look
// at getter methods in ash::BrowserContextHelper.

#include "base/component_export.h"

namespace base {
class FilePath;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {

// Base name of the signin browser context.
COMPONENT_EXPORT(ASH_BROWSER_CONTEXT_HELPER)
extern const char kSigninBrowserContextBaseName[];

// Base name of the lock-screen-app browser context.
COMPONENT_EXPORT(ASH_BROWSER_CONTEXT_HELPER)
extern const char kLockScreenAppBrowserContextBaseName[];

// Base name of the lock-screen browser context.
COMPONENT_EXPORT(ASH_BROWSER_CONTEXT_HELPER)
extern const char kLockScreenBrowserContextBaseName[];

// Base name of the shimless-rma-app browser context.
COMPONENT_EXPORT(ASH_BROWSER_CONTEXT_HELPER)
extern const char kShimlessRmaAppBrowserContextBaseName[];

// Returns true if given |browser_context| is for signin.
// Returns false if nullptr is given.
COMPONENT_EXPORT(ASH_BROWSER_CONTEXT_HELPER)
bool IsSigninBrowserContext(content::BrowserContext* browser_context);

// Returns true if given |browser_context| is for Lock-screen-app.
// Returns false if nullptr is given.
COMPONENT_EXPORT(ASH_BROWSER_CONTEXT_HELPER)
bool IsLockScreenAppBrowserContext(content::BrowserContext* browser_context);

// Returns true if given |browser_context| is for Lock-screen.
// Returns false if nullptr is given.
COMPONENT_EXPORT(ASH_BROWSER_CONTEXT_HELPER)
bool IsLockScreenBrowserContext(content::BrowserContext* browser_context);

// Returns true if given |browser_context| is for shimless-rma-app.
// Returns false if nullptr is given.
COMPONENT_EXPORT(ASH_BROWSER_CONTEXT_HELPER)
bool IsShimlessRmaAppBrowserContext(content::BrowserContext* browser_context);

// Returns true if the given |browser_context| is none of these special
// BrowserContext instances. Returns false if nullptr is given.
// Note: System and Guest Profiles are considered User BrowserContext.
// To check on that `Profile` specific method that checks the profile type
// should be used such as `Profile::IsRegularProfile()` or
// `Profile::IsSystemProfile()`.
COMPONENT_EXPORT(ASH_BROWSER_CONTEXT_HELPER)
bool IsUserBrowserContext(content::BrowserContext* browser_context);

// Returns true if the given |base_name| that is for BrowserContext directory's
// base name is for a user's (i.e. none of these special BrowserContexts').
COMPONENT_EXPORT(ASH_BROWSER_CONTEXT_HELPER)
bool IsUserBrowserContextBaseName(const base::FilePath& base_name);

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_BROWSER_CONTEXT_HELPER_BROWSER_CONTEXT_TYPES_H_
