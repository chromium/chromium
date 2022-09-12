// Copyright 2022 The Chromium Authors
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

class ProfileHelperImpl;

// This helper class is used to keep tracking the relationship between User
// and BrowserContext (a.k.a. Profile).
class COMPONENT_EXPORT(ASH_BROWSER_CONTEXT_HELPER) BrowserContextHelper {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Returns a BrowserContext object corresponding to the given path if fully
    // initialized. Otherwise returns nullptr. If the system is not
    // initialized, also returns nullptr (for unittests).
    virtual content::BrowserContext* GetBrowserContextByPath(
        const base::FilePath& path) = 0;

    // DEPRECATED. Please do not use this in the new code, and instead use
    // GetProfileByPath().
    // Similar to GetBrowserContextByPath, but synchronously create a
    // BrowserContext instance if it is not initialized.
    // If the system is not initialized, still returns nullptr (for unittests).
    // TODO(crbug.com/1325210): Remove this later.
    virtual content::BrowserContext* DeprecatedGetBrowserContext(
        const base::FilePath& path) = 0;

    // Returns the path to the user data directory.
    // If the system is not initialized, returns nullptr (for unittests).
    virtual const base::FilePath* GetUserDataDir() = 0;
  };

  explicit BrowserContextHelper(std::unique_ptr<Delegate> delegate);
  BrowserContextHelper(const BrowserContextHelper&) = delete;
  BrowserContextHelper& operator=(const BrowserContextHelper&) = delete;
  ~BrowserContextHelper();

  // BrowserContextHelper is effectively a singleton in the system.
  // This returns the pointer if already initialized.
  static BrowserContextHelper* Get();

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

  // Returns browser context path that corresponds to the given |user_id_hash|.
  base::FilePath GetBrowserContextPathByUserIdHash(
      base::StringPiece user_id_hash);

 private:
  // This is only for graceful migration.
  // TODO(crbug.com/1325210): Remove this when migration is done.
  friend class ash::ProfileHelperImpl;
  Delegate* delegate() { return delegate_.get(); }

  std::unique_ptr<Delegate> delegate_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_BROWSER_CONTEXT_HELPER_BROWSER_CONTEXT_HELPER_H_
