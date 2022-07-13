// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BROWSER_CONTEXT_HELPER_BROWSER_CONTEXT_HELPER_H_
#define CHROMEOS_ASH_COMPONENTS_BROWSER_CONTEXT_HELPER_BROWSER_CONTEXT_HELPER_H_

#include <string>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/strings/string_piece.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {

// This helper class is used to keep tracking the relationship between User
// and BrowserContext (a.k.a. Profile).
class COMPONENT_EXPORT(ASH_BROWSER_CONTEXT_HELPER) BrowserContextHelper {
 public:
  // TODO(crbug.com/1325210): Currently, static methods only.
  // Support ctor/dtor when instance of this class is needed.

  // Returns user id hash for |browser_context|, or empty string if the hash
  // could not be extracted from the |browser_context|.
  static std::string GetUserIdHashFromBrowserContext(
      content::BrowserContext* browser_context);

  // Legacy profile dir that was used when only one cryptohome has been mounted.
  static const char kLegacyBrowserContextDirName[];

  // This must be kept in sync with TestingProfile::kTestUserProfileDir.
  static const char kTestUserBrowserContextDirName[];

  // Returns user browser context dir in a format of "u-${user_id_hash}".
  static std::string GetUserBrowserContextDirName(
      base::StringPiece user_id_hash);
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_BROWSER_CONTEXT_HELPER_BROWSER_CONTEXT_HELPER_H_
